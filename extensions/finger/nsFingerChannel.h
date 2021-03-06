/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Brian Ryner.
 * Portions created by Brian Ryner are Copyright (C) 2000 Brian Ryner.
 * All Rights Reserved.
 *
 * Contributor(s): 
 *   Brian Ryner <bryner@brianryner.com>
 *   Darin Fisher <darin@netscape.com>
 */

#ifndef nsFingerChannel_h__
#define nsFingerChannel_h__

#include "nsFingerHandler.h"
#include "nsString.h"
#include "nsCOMPtr.h"

#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProgressEventSink.h"
#include "nsIInputStreamPump.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIStreamListener.h"
#include "nsISocketTransport.h"
#include "nsIProxyInfo.h"

//-----------------------------------------------------------------------------

class nsFingerChannel : public nsIChannel
                      , public nsIStreamListener
                      , public nsITransportEventSink
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUEST
    NS_DECL_NSICHANNEL
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSITRANSPORTEVENTSINK

    // nsFingerChannel methods:
    nsFingerChannel();
    virtual ~nsFingerChannel();
    
    nsresult Init(nsIURI *uri, nsIProxyInfo *proxyInfo);

protected:

    nsresult WriteRequest(nsITransport *transport);

    nsCOMPtr<nsIURI>                    mURI;
    nsCOMPtr<nsIURI>                    mOriginalURI;
    nsCOMPtr<nsIInterfaceRequestor>     mCallbacks;
    nsCOMPtr<nsIProgressEventSink>      mProgressSink;
    nsCOMPtr<nsISupports>               mOwner; 
    nsCOMPtr<nsILoadGroup>              mLoadGroup;
    nsCOMPtr<nsIStreamListener>         mListener;
    nsCOMPtr<nsISupports>               mListenerContext;
    nsCString                           mContentType;
    nsCString                           mContentCharset;
    PRUint32                            mLoadFlags;
    nsresult                            mStatus;

    nsCOMPtr<nsIInputStreamPump>        mPump;
    nsCOMPtr<nsISocketTransport>        mTransport;
    nsCOMPtr<nsIProxyInfo>              mProxyInfo;

    PRInt32                             mPort;
    nsCString                           mHost;
    nsCString                           mUser;
};

#endif // !nsFingerChannel_h__
