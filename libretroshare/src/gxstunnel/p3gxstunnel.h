/*
 * libretroshare/src/chat: distantchat.h
 *
 * Services for RetroShare.
 *
 * Copyright 2015 by Cyril Soler
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 * Please report all bugs and problems to "csoler@users.sourceforge.net".
 *
 */

#pragma once

// Generic tunnel service
//
// Preconditions:
//	* multiple services can use the same tunnel
//	* tunnels are automatically encrypted and ensure transport (items stored in a queue until ACKed by the other side)
//	* each tunnel is associated to a specific GXS id on both sides. Consequently, services that request tunnels from different IDs to a 
//		server for the same GXS id need to be handled correctly.
//
// GUI
//	* the GUI should show for each tunnel:
//		- starting and ending GXS ids
//		- tunnel status (DH ok, closed from distant peer, locally closed, etc)
//		- amount of data that is transferred in the tunnel
//		- number of pending items (and total size)
//		- number ACKed items both ways.
//
//	we can use an additional tab "Authenticated tunnels" in the statistics->turtle window
//
// Interaction with services:
//
//	Services request tunnels from a given GXS id and to a given GXS id. When ready, they get a handle (virtual peer id)
//
//	Services send data in the tunnel using the virtual peer id
//
//      Data is send to a service ID (could be any existing service ID). The endpoint of the tunnel must register each service, in order to
//	allow the data to be transmitted/sent from/to that service. Otherwise an error is issued.

#include <turtle/turtleclientservice.h>
#include <retroshare/rsgxstunnel.h>
#include <gxstunnel/rsgxstunnelitems.h>

class RsGixs ;

static const uint32_t GXS_TUNNEL_AES_KEY_SIZE = 16 ;

class p3GxsTunnelService: public RsGxsTunnelService, public RsTurtleClientService
{
public:
    p3GxsTunnelService(RsGixs *pids)
            : mGixs(pids), mGxsTunnelMtx("GXS tunnel")
    {
        mTurtle = NULL ;
    }

    virtual void connectToTurtleRouter(p3turtle *) ;

    // Creates the invite if the public key of the distant peer is available.
    // Om success, stores the invite in the map above, so that we can respond to tunnel requests.
    //
    virtual bool requestSecuredTunnel(const RsGxsId& to_id,const RsGxsId& from_id,RsGxsTunnelClientService *client,uint32_t& error_code) ;
    virtual bool closeExistingTunnel(const RsGxsId& pid) ;
    virtual bool getTunnelStatus(const RsGxsId &gxs_id,uint32_t &status, RsGxsId *from_gxs_id=NULL) ;

private:
    void flush() ;
    virtual bool handleIncomingItem(RsGxsTunnelItem *) ;
    
    class GxsTunnelPeerInfo
    {
    public:
        GxsTunnelPeerInfo() : last_contact(0), last_keep_alive_sent(0), status(0), direction(0)
        {
            memset(aes_key, 0, GXS_TUNNEL_AES_KEY_SIZE);
        }

        time_t last_contact ; 		// used to keep track of working connexion
	time_t last_keep_alive_sent ;	// last time we sent a keep alive packet.

        unsigned char aes_key[GXS_TUNNEL_AES_KEY_SIZE] ;

        uint32_t status ;		// info: do we have a tunnel ?
        RsPeerId virtual_peer_id;  	// given by the turtle router. Identifies the tunnel.
        RsGxsId own_gxs_id ;         	// gxs id we're using to talk.
        RsTurtleGenericTunnelItem::Direction direction ; // specifiec wether we are client(managing the tunnel) or server.
    };

    class GxsTunnelDHInfo
    {
    public:
        GxsTunnelDHInfo() : dh(0), direction(0), status(0) {}

        DH *dh ;
        RsGxsId gxs_id ;
        RsTurtleGenericTunnelItem::Direction direction ;
	uint32_t status ;
	TurtleFileHash hash ;
    };

    // This maps contains the current peers to talk to with distant chat.
    //
    std::map<RsGxsId, GxsTunnelPeerInfo> 	_gxs_tunnel_contacts ;		// current peers we can talk to
    std::map<RsPeerId,GxsTunnelDHInfo>    	_gxs_tunnel_virtual_peer_ids ;	// current virtual peers. Used to figure out tunnels, etc.

    // List of items to be sent asap. Used to store items that we cannot pass directly to
    // sendTurtleData(), because of Mutex protection.

    std::list<RsGxsTunnelItem*> pendingGxsTunnelItems ;

    // Overloaded from RsTurtleClientService

    virtual bool handleTunnelRequest(const RsFileHash &hash,const RsPeerId& peer_id) ;
    virtual void receiveTurtleData(RsTurtleGenericTunnelItem *item,const RsFileHash& hash,const RsPeerId& virtual_peer_id,RsTurtleGenericTunnelItem::Direction direction) ;
    void addVirtualPeer(const TurtleFileHash&, const TurtleVirtualPeerId&,RsTurtleGenericTunnelItem::Direction dir) ;
    void removeVirtualPeer(const TurtleFileHash&, const TurtleVirtualPeerId&) ;
    
    // session handling handles
    
    void markGxsTunnelAsClosed(const RsGxsId &gxs_id) ;
    void startClientGxsTunnelConnection(const RsGxsId &to_gxs_id, const RsGxsId& from_gxs_id) ;
    void locked_restartDHSession(const RsPeerId &virtual_peer_id, const RsGxsId &own_gxs_id) ;

    // utility functions

    static TurtleFileHash hashFromGxsId(const RsGxsId& destination) ;
    static RsGxsId gxsIdFromHash(const TurtleFileHash& sum) ;

    // Cryptography management
    
    void handleRecvDHPublicKey(RsGxsTunnelDHPublicKeyItem *item) ;
    bool locked_sendDHPublicKey(const DH *dh, const RsGxsId& own_gxs_id, const RsPeerId& virtual_peer_id) ;
    bool locked_initDHSessionKey(DH *&dh);
    
    TurtleVirtualPeerId virtualPeerIdFromHash(const TurtleFileHash& hash) ;	// ... and to a hash for p3turtle

    // item handling
    
    void handleRecvStatusItem(RsGxsTunnelStatusItem *item) ;
    
    // Comunication with Turtle service

    void sendTurtleData(RsGxsTunnelItem *) ;
    void sendEncryptedTurtleData(const uint8_t *buff,uint32_t rssize,const RsGxsId &gxs_id) ;
    bool handleEncryptedData(const uint8_t *data_bytes,uint32_t data_size,const TurtleFileHash& hash,const RsPeerId& virtual_peer_id) ;

    static TurtleFileHash hashFromVirtualPeerId(const DistantChatPeerId& peerId) ;	// converts IDs so that we can talk to RsPeerId from outside

    // local data
    
    p3turtle 	*mTurtle ;
    RsGixs 	*mGixs ;
    RsMutex  	 mGxsTunnelMtx ;
};
