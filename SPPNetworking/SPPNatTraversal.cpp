// Copyright (c) David Sleeper (Sleeping Robot LLC)
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.

#include "SPPNatTraversal.h"
#include "SPPLogging.h"
#include "SPPSerialization.h"
#include "SPPDatabase.h"
#include "SPPTiming.h"


#include "json/json.h"
#include "juice/juice.h"

extern "C"
{
#include "juice/base64.h"
}

#include <iostream>
#include <thread>
#include <mutex>

namespace SPP
{
	LogEntry LOG_JUICE("juice");
	LogEntry LOG_COORD("coord");

	////////////////////////////////
	//UDP_SQL_Coordinator
	////////////////////////////////
	
	struct UDP_SQL_Coordinator::PlatImpl
	{
		bool IsHost = false;

		IPv4_SocketAddress RemoteAddr;
		std::unique_ptr< SQLLiteDatabase > DBConnection;
		std::unique_ptr< UDPSocket > CoordinatorSocket;

		std::vector<uint8_t> BufferRead;
		SystemClock::time_point LastSendTime;
		SystemClock::time_point LastRecvUpdateFromRemote;

		std::map<std::string, std::string> KeyMapping;
		std::function<void(const std::string&)> ResponseFunc;

		std::map<std::string, ESQLFieldType> FieldMap;

		PlatImpl()
		{
			BufferRead.resize(10 * 1024);
			LastSendTime = SystemClock::now() + std::chrono::seconds(1);
		}
	};

	UDP_SQL_Coordinator::UDP_SQL_Coordinator(const IPv4_SocketAddress &InRemoteAddr) : _impl(new PlatImpl())
	{
		_impl->CoordinatorSocket = std::make_unique < UDPSocket >();
		_impl->RemoteAddr = InRemoteAddr;
	}

	UDP_SQL_Coordinator::UDP_SQL_Coordinator(uint16_t InPort, const std::vector<TableField>& InFields) : _impl(new PlatImpl())
	{
		_impl->IsHost = true;		
		_impl->CoordinatorSocket = std::make_unique < UDPSocket >(InPort);

		_impl->DBConnection = std::make_unique< SQLLiteDatabase >();
		_impl->DBConnection->Connect("coordDB.db");
		_impl->DBConnection->GenerateTable("clients", InFields);

		for (auto& field : InFields)
		{
			_impl->FieldMap[field.Name] = field.Type;
		}
	}

	UDP_SQL_Coordinator::~UDP_SQL_Coordinator()
	{
		if (_impl->DBConnection)
		{
			_impl->DBConnection->Close();
			_impl->DBConnection.reset();
		}
	}

	void UDP_SQL_Coordinator::SetKeyPair(const std::string& Key, const std::string& Value)
	{
		_impl->KeyMapping[Key] = Value;
	}

	void UDP_SQL_Coordinator::GetLocalKeyValue(const std::string& Key, std::string& Value)
	{
		auto findKey = _impl->KeyMapping.find(Key);
		if (findKey != _impl->KeyMapping.end())
		{
			Value = findKey->second;
		}
	}

	void UDP_SQL_Coordinator::SetSQLRequestCallback(std::function<void(const std::string&)> InReponseFunc)
	{
		_impl->ResponseFunc = InReponseFunc;
	}

	void UDP_SQL_Coordinator::SQLRequest(const std::string& InSQL)
	{
		if (_impl->IsHost)
		{
			Json::Value SQLResults;

			_impl->DBConnection->RunSQL(InSQL.c_str(), [&SQLResults](int argc, char** argv, char** azColName) -> int
				{
					Json::Value SQLResult;
					for (int32_t Iter = 0; Iter < argc; Iter++)
					{
						SQLResult[azColName[Iter]] = argv[Iter];
					}
					SQLResults.append(SQLResult);
					return 0;
				});
			
			if (_impl->ResponseFunc)
			{
				Json::StreamWriterBuilder wbuilder;
				std::string StrMessage = Json::writeString(wbuilder, SQLResults);
				_impl->ResponseFunc(StrMessage);
			}
		}
		else
		{
			Json::Value JsonMessage;

			char SQLString[1024] = { 0 };
			juice_base64_encode(InSQL.data(), InSQL.length(), SQLString, sizeof(SQLString));
			JsonMessage["SQL"] = std::string(SQLString);

			Json::StreamWriterBuilder wbuilder;
			std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
			_impl->CoordinatorSocket->SendTo(_impl->RemoteAddr, StrMessage.c_str(), StrMessage.size());
		}
	}

	bool UDP_SQL_Coordinator::IsConnected() const
	{
		return std::chrono::duration_cast<std::chrono::seconds>(SystemClock::now() - _impl->LastRecvUpdateFromRemote).count() < 6;
	}

	void UDP_SQL_Coordinator::Update()
	{
		auto CurrentTime = SystemClock::now();

		//RECEIVING DATA
		{
			IPv4_SocketAddress currentAddress;
			auto packetSize = _impl->CoordinatorSocket->ReceiveFrom(currentAddress, _impl->BufferRead.data(), _impl->BufferRead.size());

			if (packetSize > 0)
			{
				Json::Value root;
				Json::CharReaderBuilder Builder;
				Json::CharReader* reader = Builder.newCharReader();
				std::string Errors;

				bool parsingSuccessful = reader->parse((char*)_impl->BufferRead.data(), (char*)(_impl->BufferRead.data() + packetSize), &root, &Errors);
				delete reader;
				if (parsingSuccessful)
				{
					if (_impl->IsHost)
					{
						Json::Value HasSQLRequest = root.get("SQL", Json::Value::nullSingleton());
						Json::Value SQLResults;

						// if not SQL it always a push
						if (HasSQLRequest.isNull())
						{
							std::string ColumnNames;
							std::string ColumnValues;

							for (Json::Value::const_iterator itr = root.begin(); itr != root.end(); itr++)
							{
								std::string ColumnName = itr.memberName();
								std::string ColumnValue = root[ColumnName.c_str()].asCString();

								if (!ColumnNames.empty())ColumnNames += ", ";
								ColumnNames += std::string_format("%s", ColumnName.c_str());

								if (!ColumnValues.empty())ColumnValues += ", ";

								auto fieldType = _impl->FieldMap[ColumnName];
								if (fieldType == ESQLFieldType::Direct)
								{
									ColumnValues += std::string_format("%s", ColumnValue.c_str());
								}
								else
								{
									ColumnValues += std::string_format("'%s'", ColumnValue.c_str());
								}
							}

							std::string SQLCommand = std::string_format("REPLACE INTO clients(%s) VALUES(%s);", ColumnNames.c_str(), ColumnValues.c_str() );							
							_impl->DBConnection->RunSQL(SQLCommand.c_str());
						}
						else
						{							
							char sqlDecode[1024] = { 0 };
							juice_base64_decode(HasSQLRequest.asCString(), sqlDecode, sizeof(sqlDecode));

							_impl->DBConnection->RunSQL(sqlDecode, [&SQLResults](int argc, char** argv, char** azColName) -> int
								{
									Json::Value SQLResult;
									for (int32_t Iter = 0; Iter < argc; Iter++)
									{
										if (argv[Iter])
										{
											SQLResult[azColName[Iter]] = argv[Iter];
										}
									}
									SQLResults.append(SQLResult);
									return 0;
								});
						}

						//send a message back about time
						Json::Value JsonMessage;
						JsonMessage["SERVERTIME"] = TimePointToString<SystemClock>(CurrentTime, "UTC: %Y-%m-%d %H:%M:%S");
						if (!SQLResults.isNull())
						{
							JsonMessage["SQLRESULT"] = SQLResults;
						}
						Json::StreamWriterBuilder wbuilder;
						std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
						_impl->CoordinatorSocket->SendTo(currentAddress, StrMessage.c_str(), StrMessage.size());
					}
					else
					{
						Json::Value ServerTime = root.get("SERVERTIME", Json::Value::nullSingleton());
						if (ServerTime.isNull() == false)
						{
							_impl->LastRecvUpdateFromRemote = SystemClock::now();
						}

						Json::Value SQLResultValue = root.get("SQLRESULT", Json::Value::nullSingleton());
						if (SQLResultValue.isNull() == false)
						{
							if (_impl->ResponseFunc)
							{
								Json::StreamWriterBuilder wbuilder;
								std::string StrMessage = Json::writeString(wbuilder, SQLResultValue);
								_impl->ResponseFunc(StrMessage);
							}							
						}
					}
				}
			}
		}

		// if its not the host send who we are
		if (!_impl->IsHost)
		{
			if (std::chrono::duration_cast<std::chrono::seconds>(CurrentTime - _impl->LastSendTime).count() > 2)
			{
				Json::Value JsonMessage;

				for (auto& [key, value] : _impl->KeyMapping)
				{
					JsonMessage[key] = value;
				}

				Json::StreamWriterBuilder wbuilder;
				std::string StrMessage = Json::writeString(wbuilder, JsonMessage);
				_impl->CoordinatorSocket->SendTo(_impl->RemoteAddr, StrMessage.c_str(), StrMessage.size());
				_impl->LastSendTime = CurrentTime;
			}
		}
	}

	////////////////////////////////
	//UDPJuiceSocket
	////////////////////////////////


	struct UDPJuiceSocket::PlatImpl
	{
		juice_agent_t* agent = nullptr;
		juice_config_t config = { 0 };
		std::atomic_bool bDoneGathering = false;
		bool bHasSetRemoteSDP = false;
		char sdp[JUICE_MAX_SDP_STRING_LEN];
		char sdp64[JUICE_MAX_SDP_STRING_LEN*2];

		std::mutex incDataMutex;
		BinaryBlobSerializer incData;

		SystemClock::time_point ConnectedStartTime;
	};

	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr)
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);

		SPP_LOG(LOG_JUICE, LOG_INFO, "on_state_changed: %s", juice_state_to_string(state));

		if (state == JUICE_STATE_CONNECTED)
		{
			// Agent 1: on connected, send a message
			//const char* message = "NAT traversed peer message!!!!!!";
			//juice_send(gent, message, strlen(message) + 1);
		}
	}

	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr)
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		SPP_LOG(LOG_JUICE, LOG_INFO, "on_candidate: %s", sdp);
		// Agent 2: Receive it from agent 1
		//juice_add_remote_candidate(agent2, sdp);
	}

	static void on_gathering_done(juice_agent_t* agent, void* user_ptr) 
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		SPP_LOG(LOG_JUICE, LOG_INFO, "on_gathering_done");
		juiceSocket->INTERNAL_DoneGathering();
		//juice_set_remote_gathering_done(agent2); // optional
	}

	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) 
	{
		auto juiceSocket = static_cast<UDPJuiceSocket*>(user_ptr);
		//SPP_LOG(LOG_JUICE, LOG_INFO, "on_recv: %d", size);
		juiceSocket->INTERNAL_DataRecv(data, size);
	}

	UDPJuiceSocket::UDPJuiceSocket(const char* Addr, uint16_t InPort) : _impl(new PlatImpl())
	{
		_impl->config.stun_server_host = Addr;
		_impl->config.stun_server_port = InPort;
		_impl->config.turn_servers_count = 0;

		_impl->config.cb_state_changed = on_state_changed;
		_impl->config.cb_candidate = on_candidate;
		_impl->config.cb_gathering_done = on_gathering_done;
		_impl->config.cb_recv = on_recv;
		_impl->config.user_ptr = this;

		_impl->agent = juice_create(&_impl->config);

		juice_get_local_description(_impl->agent, _impl->sdp, JUICE_MAX_SDP_STRING_LEN);
		juice_gather_candidates(_impl->agent);	
	}

	void UDPJuiceSocket::INTERNAL_DataRecv(const char* data, size_t size)
	{
		std::unique_lock<std::mutex> lk(_impl->incDataMutex);
		_impl->incData.Write(data, size);
	}	

	void UDPJuiceSocket::INTERNAL_DoneGathering()
	{
		juice_get_local_description(_impl->agent, _impl->sdp, JUICE_MAX_SDP_STRING_LEN);
		juice_base64_encode(_impl->sdp, std::strlen(_impl->sdp), _impl->sdp64, sizeof(_impl->sdp64));

		_impl->bDoneGathering = true;
		SPP_LOG(LOG_JUICE, LOG_INFO, "\n*** REMOTE SIGNATURE ***\n%s", _impl->sdp);
	}

	bool UDPJuiceSocket::IsReady() const
	{
		return _impl->bDoneGathering;
	}

	const char* UDPJuiceSocket::GetSDP() const
	{
		return _impl->sdp;
	}

	const char* UDPJuiceSocket::GetSDP_BASE64() const
	{
		return _impl->sdp64;
	}

	UDPJuiceSocket::~UDPJuiceSocket()
	{
		juice_destroy(_impl->agent);
	}

	void UDPJuiceSocket::Send(const void* buf, uint16_t BufferSize) 
	{
		//SPP_LOG(LOG_JUICE, LOG_INFO, "juice_send: %d", BufferSize);
		juice_send(_impl->agent, (const char*) buf, BufferSize);
	}

	int32_t UDPJuiceSocket::Receive(void* buf, uint16_t InBufferSize) 
	{
		std::unique_lock<std::mutex> lk(_impl->incDataMutex);
		uint16_t maxWrite = (uint16_t)std::min<uint64_t>(std::numeric_limits<uint16_t>::max(), _impl->incData.Size());
		if(maxWrite)
		{
			// copy that front
			auto& IncomingData = _impl->incData.GetArray();
			memcpy(buf, IncomingData.data(), maxWrite);
			// erase what we wrote
			IncomingData.erase(IncomingData.begin(), IncomingData.begin() + maxWrite);
			// fix size after erase
			_impl->incData.Seek(_impl->incData.Size());
		}
		return maxWrite;
	}

	bool UDPJuiceSocket::IsConnected() 
	{
		auto currentState = juice_get_state(_impl->agent);
		return (currentState == JUICE_STATE_COMPLETED ||
			currentState == JUICE_STATE_CONNECTED);
	}

	bool UDPJuiceSocket::HasProblem()
	{
		auto currentState = juice_get_state(_impl->agent);

		// straight error
		if (currentState == JUICE_STATE_FAILED)
		{
			SPP_LOG(LOG_JUICE, LOG_INFO, "DPJuiceSocket::HasProblem:: FAILED", _impl->sdp);
			return true;
		}

		// connecting for toooo long
		if (currentState == JUICE_STATE_CONNECTING &&
			_impl->bHasSetRemoteSDP &&
			std::chrono::duration_cast<std::chrono::seconds>(SystemClock::now() - _impl->ConnectedStartTime).count() > 15)
		{
			SPP_LOG(LOG_JUICE, LOG_INFO, "DPJuiceSocket::HasProblem:: CONNECTING TIMEOUT", _impl->sdp);
			return true;
		}

		return false;
	}

	bool UDPJuiceSocket::HasRemoteSDP() const
	{
		return _impl->bHasSetRemoteSDP;
	}

	void UDPJuiceSocket::SetRemoteSDP(const char* InDesc)
	{
		if (_impl->bHasSetRemoteSDP == false)
		{
			juice_set_remote_description(_impl->agent, InDesc);
			_impl->bHasSetRemoteSDP = true;
			_impl->ConnectedStartTime = SystemClock::now();
		}
	}

	void UDPJuiceSocket::SetRemoteSDP_BASE64(const char* InDesc)
	{
		if (_impl->bHasSetRemoteSDP == false)
		{
			char remotesdp[JUICE_MAX_SDP_STRING_LEN] = { 0 };
			auto decodePos = juice_base64_decode(InDesc, remotesdp, sizeof(remotesdp));

			if (decodePos > 0)
			{
				juice_set_remote_description(_impl->agent, remotesdp);
				_impl->bHasSetRemoteSDP = true;
				_impl->ConnectedStartTime = SystemClock::now();

				SPP_LOG(LOG_JUICE, LOG_INFO, "SetRemoteSDP_BASE64(decoded): \n%s", remotesdp);
			}
			else
			{
				SPP_LOG(LOG_JUICE, LOG_INFO, "SetRemoteSDP_BASE64(decoded): COULD NOT DECODE");
			}
		}
	}

}
	

