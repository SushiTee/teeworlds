/* Webapp class by Sushi and Redix */

#include <base/math.h>

#include <stdio.h>

#include "http_con.h"

// TODO: support for chunked transfer-encoding?
// TODO: error handling
bool CHttpConnection::CHeader::Parse(char *pStr)
{
	char *pEnd = (char*)str_find(pStr, "\r\n\r\n");
	if(!pEnd)
		return false;
	
	*(pEnd+2) = 0;
	char *pData = pStr;
	
	if(sscanf(pData, "HTTP/%*d.%*d %d %*s\r\n", &this->m_StatusCode) != 1)
	{
		m_Error = true;
		return false;
	}
	
	while(sscanf(pData, "Content-Length: %ld\r\n", &this->m_ContentLength) != 1)
	{
		char *pLineEnd = (char*)str_find(pData, "\r\n");
		if(!pLineEnd)
		{
			m_Error = true;
			return false;
		}
		pData = pLineEnd + 2;
	}
	
	m_Size = (pEnd-pStr)+4;
	return true;
}

CHttpConnection::~CHttpConnection()
{
	if(m_pResponse)
		delete m_pResponse;
	if(m_pRequest)
		mem_free(m_pRequest);
	if(m_RequestFile)
		io_close(m_RequestFile);
	if(m_pUserData)
		delete m_pUserData;
	Close();
}

bool CHttpConnection::Create(NETADDR Addr, int Type, IStream *pResponse)
{
	m_Addr = Addr;
	m_Socket = net_tcp_create(m_Addr);
	if(m_Socket.type == NETTYPE_INVALID)
		return false;

	net_set_non_blocking(m_Socket);
	m_State = STATE_CONNECT;
	m_Type = Type;
	m_pResponse = pResponse;
	return true;
}

void CHttpConnection::Close()
{
	net_tcp_close(m_Socket);
	m_State = STATE_NONE;
}

void CHttpConnection::SetRequest(const char *pData, int Size, IOHANDLE RequestFile)
{
	m_RequestSize = Size;
	m_pRequest = (char *)mem_alloc(m_RequestSize, 1);
	mem_copy(m_pRequest, pData, m_RequestSize);
	m_pRequestCur = m_pRequest;
	m_pRequestEnd = m_pRequest + m_RequestSize;
	m_RequestFile = RequestFile;
}

int CHttpConnection::Update()
{
	if(time_get() - m_LastActionTime > time_freq() * 5 && m_LastActionTime != -1)
	{
		dbg_msg("webapp", "timeout (type: %d)", m_Type);
		return -1;
	}

	switch(m_State)
	{
		case STATE_CONNECT:
		{
			if(time_get() < m_StartTime)
				return 0;

			m_State = STATE_WAIT;
			if(net_tcp_connect(m_Socket, &m_Addr) != 0)
			{
				if(net_in_progress())
					m_LastActionTime = time_get();
				return net_in_progress() ? 0 : -1;
			}
			return 0;
		}

		case STATE_WAIT:
		{
			int Result = net_socket_write_wait(m_Socket, 0);
			if(Result == 1)
			{
				dbg_msg("webapp", "connected (type: %d)", m_Type);
				m_LastActionTime = time_get();
				m_State = STATE_SEND;
				return 0;
			}
			return Result;
		}

		case STATE_SEND:
		{
			if(m_pRequestCur >= m_pRequestEnd)
			{
				if(m_RequestFile)
				{
					char aData[1024];
					unsigned Bytes = io_read(m_RequestFile, aData, sizeof(aData));
					if(Bytes > 0)
					{
						int Send = net_tcp_send(m_Socket, aData, Bytes);
						if(Send != Bytes)
							return -1;
						m_LastActionTime = time_get();
						return 0;
					}
					else // not nice but it works...
					{
						const char *pFooter = "\r\n--frontier--\r\n";
						int Send = net_tcp_send(m_Socket, pFooter, str_length(pFooter));
						if(Send != str_length(pFooter))
							return -1;
						m_LastActionTime = time_get();
					}
				}
				dbg_msg("webapp", "sent request (type: %d)", m_Type);
				m_State = STATE_RECV;
			}
			else
			{
				int Send = net_tcp_send(m_Socket, m_pRequestCur, min(1024, m_pRequestEnd-m_pRequestCur));
				if(Send < 0)
					return -1;
				m_LastActionTime = time_get();
				m_pRequestCur += Send;
			}
			return 0;
		}

		case STATE_RECV:
		{
			char aBuf[1024] = {0};
			int Bytes = net_tcp_recv(m_Socket, aBuf, sizeof(aBuf));

			if(Bytes > 0)
			{
				m_LastActionTime = time_get();
				if(m_Header.m_Size == -1)
				{
					m_HeaderBuffer.Write(aBuf, Bytes);
					if(m_Header.Parse(m_HeaderBuffer.GetData()))
					{
						if(m_Header.m_Error)
							return -1;
						else
							m_pResponse->Write(m_HeaderBuffer.GetData()+m_Header.m_Size, m_HeaderBuffer.Size()-m_Header.m_Size);
					}
				}
				else
				{
					if(!m_pResponse->Write(aBuf, Bytes))
						return -1;
				}
			}
			else if(Bytes < 0)
			{
				return net_would_block() ? 0 : -1;
			}
			else
			{
				if(m_Header.m_StatusCode != 0 && m_Header.m_StatusCode != 200)
					return -m_Header.m_StatusCode;
				return m_pResponse->Size() == m_Header.m_ContentLength ? 1 : -1;
			}
			return 0;
		}

		default:
			return -1;
	}
}
