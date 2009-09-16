/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2008 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_NETWORKMANAGER_SESSION_H
#define ANH_NETWORKMANAGER_SESSION_H

//#include "Utils/queue.h"
#include "Utils/Clock.h"
#include "zthread/ZThread.h"
#include "Utils/typedefs.h"
#include "LogManager/LogManager.h"
#include "Common/Message.h"
#include "NetConfig.h"
#include <list>
#include <queue>
#include <boost/asio.hpp>


//======================================================================================================================

class NetworkClient;
class Service;
class SocketReadThread;
class SocketWriteThread;
class PacketFactory;
class MessageFactory;
class Packet;
class SessionPacket;

using boost::asio::ip::udp;
//======================================================================================================================

typedef std::list<Packet*,std::allocator<Packet*> >		PacketWindowList;
typedef std::queue<Packet*>								PacketQueue;
//typedef std::priority_queue<Message*,std::vector<Message*>,CompareMsg>  MessageQueue;
typedef std::queue<Message*>							MessageQueue;

//======================================================================================================================

enum SessionStatus
{
	SSTAT_Initialize = 0,
	SSTAT_Connecting,
	SSTAT_Connected,
	SSTAT_Disconnecting,
	SSTAT_Disconnected,
	SSTAT_Destroy,
	SSTAT_Timeout,
	SSTAT_Error
};

enum SessionCommand
{
	SCOM_None = 0,
	SCOM_Connect,
	SCOM_Disconnect
};

//======================================================================================================================

class Session
{
	public:
								  Session(void);
								  ~Session(void);

	  void                        ProcessReadThread(void);
	  void                        ProcessWriteThread(void);

	  void                        HandleSessionPacket(Packet* packet);
	  void                        SortSessionPacket(Packet* packet, uint16 type);
	  void                        HandleFastpathPacket(Packet* packet);
	  
	  void                        SendChannelA(Message* message);
	
	  void						  SendChannelAUnreliable(Message* message);
	  void                        DestroyIncomingMessage(Message* message);
	  void                        DestroyPacket(Packet* packet);
	  
	  
	  // Accessor methods
	  NetworkClient*              getClient(void)                                 { return mClient; }
	  Service*                    getService(void)                                { return mService; }
	  uint32                      getId(void)                                     { return mId; }
	  uint32                      getAddress(void)                                { return mAddress; }
	  int8*                       getAddressString(void);
	  uint16                      getPort(void)                                   { return mPort; }
	  uint16                      getPortHost(void);
	  uint32                      getOutgoingReliablePacketCount(void)            { return mOutgoingReliablePacketQueue.size(); }
	  Packet*                     getOutgoingReliablePacket(void);
	  uint32                      getOutgoingUnreliablePacketCount(void)          { return mOutgoingUnreliablePacketQueue.size(); }
	  Packet*                     getOutgoingUnreliablePacket(void);
	  uint32                      getIncomingQueueMessageCount()    { return mIncomingMessageQueue.size(); }
	  Message*                    getIncomingQueueMessage();
	  uint32                      getEncryptKey(void)                             { return mEncryptKey; }
	  SessionStatus               getStatus(void)                                 { return mStatus; }
	  SessionCommand              getCommand(void)                                { return mCommand; }
	  bool                        getInOutgoingQueue(void)                        { return mInOutgoingQueue; }
	  bool                        getInIncomingQueue(void)                        { return mInIncomingQueue; }
	  uint32					  getResendWindowSize()							  { return mWindowResendSize; }
	  udp::endpoint				  getEndpoint()									  { return mEndpoint;}


	  void						  setEndpoint(udp::endpoint point)				  { mEndpoint = point;}
	  void						  setResendWindowSize(uint32 resendWindowSize)	  { mWindowResendSize = resendWindowSize;  mWindowSizeCurrent = resendWindowSize; }
	  void                        setClient(NetworkClient* client)                { mClient = client; }
	  void                        setService(Service* service)                    { mService = service; }
	  void                        setSocketReadThread(SocketReadThread* thread)   { mSocketReadThread = thread; }
	  void                        setSocketWriteThread(SocketWriteThread* thread) { mSocketWriteThread = thread; }
	  void                        setPacketFactory(PacketFactory* factory)        { mPacketFactory = factory; }
	  void                        setMessageFactory(MessageFactory* factory)      { mMessageFactory = factory; }
	  void                        setId(uint32 id)                                { mId = id; }
	  void                        setAddress(uint32 address)                      { mAddress = address; }
	  void                        setPort(uint16 port)                            { mPort = port; }
	  void                        setEncryptKey(uint32 key)                       { mEncryptKey = key; }
	  void                        setStatus(SessionStatus status)                 { mStatus = status; }
	  void                        setCommand(SessionCommand command)              { mCommand = command; }
	  void                        setInOutgoingQueue(bool in)                     { mInOutgoingQueue = in; }
	  void                        setInIncomingQueue(bool in)                     { mInIncomingQueue = in; }
	  void                        setPacketSize(uint16 size)					  { mMaxPacketSize = size;}
	  void                        setUnreliableSize(uint16 size)				  { mMaxUnreliableSize = size;}
	 
	 
	  uint64					  mLastPacketDestroyed;

private:
	  void                        _processSessionRequestPacket(Packet* packet);
	  void                        _processDisconnectPacket(Packet* packet);
	  void                        _processMultiPacket(Packet* packet);
	  void                        _processNetStatRequestPacket(Packet* packet);
	  
	  void                        _processDataChannelPacket(Packet* packet, bool fastPath);
	  void                        _processDataChannelB(Packet* packet, bool fastPath);
	  
	  void                        _processDataOrderPacket(Packet* packet);
	  void                        _processDataOrderChannelB(Packet* packet);
	  void                        _processDataChannelAck(Packet* packet);
	  void                        _processFragmentedPacket(Packet* packet);
	  void                        _processPingPacket(Packet* packet);

	  void                        _processConnectCommand(void);
	  void                        _processDisconnectCommand(void);

	  void                        _addOutgoingMessage(Message* message, uint8 priority, bool fastpath);
	  void                        _addIncomingMessage(Message* message, uint8 priority);

	  uint32					  _buildPackets();
	  uint32					  _buildPacketsUnreliable();
	  void						  _buildMultiDataPacket();
	  void						  _buildRoutedMultiDataPacket();
	  void						  _buildUnreliableMultiDataPacket();
	  void                        _buildOutgoingReliablePackets(Message* message);
	  void						  _buildOutgoingReliableRoutedPackets(Message* message);
	  void                        _buildOutgoingUnreliablePackets(Message* message);
	  void                        _addOutgoingReliablePacket(Packet* packet);
	  void                        _addOutgoingUnreliablePacket(Packet* packet);
	  void                        _resendOutgoingPackets(void);
	  void                        _sendPingPacket(void);

	  void						  _handleOutSequenceRollover();

	  //we want to use bigger packets in the zone connection server communication!
	  uint16					  mMaxPacketSize;
	  uint16					  mMaxUnreliableSize;

	  //we want to only encrypt / pack when we communicate with the client
	  bool						  mServerService;

	  Service*                    mService;
	  NetworkClient*              mClient;
	  SocketReadThread*           mSocketReadThread;
	  SocketWriteThread*          mSocketWriteThread;
	  PacketFactory*              mPacketFactory;
	  MessageFactory*             mMessageFactory;
	  // Anh_Utils::Clock*           mClock;
	  

	  udp::endpoint 			  mEndpoint;
	  uint32                      mId;
	  uint32                      mAddress;                 // stored in network order
	  uint16                      mPort;                    // stored in network order
	  uint32                      mEncryptKey;
	  uint32                      mRequestId;
	  uint32                      mOutgoingPingSequence;

	  // Incoming fragmented packet processing.
	  uint32                      mFragmentedPacketTotalSize;
	  uint32                      mFragmentedPacketCurrentSize;
	  uint16					  mFragmentedPacketStartSequence;
	  uint16					  mFragmentedPacketCurrentSequence;

	  uint64                      mConnectStartEvent;       // For SCOM_Connect commands
	  uint64                      mLastConnectRequestSent;  

	  uint64                      mLastPacketReceived;      // General session timeout
	  uint64                      mLastPacketSent;          // General session timeout

	  // Netstats
	  uint32				      mServerTickCount;
	  uint32                      mLastRoundtripTime;
	  uint32                      mAverageRoundtripTime;
	  uint32                      mShortestRoundtripTime;
	  uint32                      mLongestRoundtripTime;
	  uint64                      mClientPacketsSent;
	  uint64                      mClientPacketsReceived;
	  uint64                      mServerPacketsSent;
	  uint64                      mServerPacketsReceived;

	  // Reliability
	  uint16                      mOutSequenceNext;
	  uint16                      mInSequenceNext;
	  
	  bool						  mOutSequenceRollover;
	  uint16                      mNextPacketSequenceSent;
	  uint64                      mLastRemotePacketAckReceived;
	  uint32                      mWindowSizeCurrent;		//amount of packets we want to send in one round
	  uint32                      mWindowResendSize;	    //

	  bool                        mSendDelayedAck;        // We processed some incoming packets, send an ack
	  bool                        mInOutgoingQueue;       // Are we already in the queue?
	  bool                        mInIncomingQueue;       // Are we already in the queue?

	  uint16                      mLastSequenceAcked;

	  SessionStatus               mStatus;
	  SessionCommand              mCommand;

	  // Message queues.
	  MessageQueue                mOutgoingMessageQueue;		//here we store the messages given to us by the messagelib
	  MessageQueue                mUnreliableMessageQueue;

	  MessageQueue                mIncomingMessageQueue;
	  MessageQueue				  mMultiMessageQueue;
	  MessageQueue				  mRoutedMultiMessageQueue;
	  MessageQueue				  mMultiUnreliableQueue;

	  // Packet queues.
	  PacketQueue                 mOutgoingReliablePacketQueue;		//these are packets put on by the sessionwrite thread to send
	  PacketQueue                 mOutgoingUnreliablePacketQueue;   //build unreliables they will get send directly by the socket write thread  without storing for possible r esends
	  PacketWindowList            mWindowPacketList;				//our build packets - ready to get send
	  PacketWindowList			  mRolloverWindowPacketList;		//send packets after a rollover they await sending and / or acknowledgement by the client
	  PacketWindowList			  mNewRolloverWindowPacketList;
	  PacketWindowList            mNewWindowPacketList;				

	  PacketQueue                 mIncomingFragmentedPacketQueue;
	  PacketWindowList            mIncomingPacketList;				
	  
	  ZThread::RecursiveMutex	  mSessionMutex;
	 
	  uint64					  lasttime;
	  uint64					  avgTime;
	  uint32                      avgPacketsbuild;
	  uint32                      avgUnreliablesbuild;

	  uint64					  mPacketBuildTimeLimit;
	  uint64					  mLastWriteThreadTime;

	  uint32					  endCount;
	  uint16					  lowest;// the lowest packet requested from the server
	  uint16					  lowestCount;// counts the requests up
	  
};


//======================================================================================================================

#endif //ANH_NETWORKMANAGER_SESSION_H




