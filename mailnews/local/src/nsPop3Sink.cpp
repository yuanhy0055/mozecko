/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   bienvenu@nventure.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...
#include "nsPop3Sink.h"
#include "prprf.h"
#include "prlog.h"
#include "nscore.h"
#include <stdio.h>
#include <time.h>
#include "nsIFileSpec.h"
#include "nsParseMailbox.h"
#include "nsIMsgLocalMailFolder.h"
#include "nsIMsgIncomingServer.h"
#include "nsLocalUtils.h"
#include "nsMsgLocalFolderHdrs.h"
#include "nsIMsgFolder.h" // TO include biffState enum. Change to bool later...
#include "nsReadableUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPop3Sink, nsIPop3Sink)

nsPop3Sink::nsPop3Sink()
{
    m_authed = PR_FALSE;
    m_accountUrl = nsnull;
    m_biffState = 0;
    m_numNewMessages = 0;
    m_numNewMessagesInFolder = 0;
    m_senderAuthed = PR_FALSE;
    m_outputBuffer = nsnull;
    m_outputBufferSize = 0;
    m_newMailParser = nsnull;
#ifdef DEBUG
    m_fileCounter = 0;
#endif
    m_popServer = nsnull;
    m_outFileStream = nsnull;
    m_folder = nsnull;
    m_buildMessageUri = PR_FALSE;
}

nsPop3Sink::~nsPop3Sink()
{
    PR_Free(m_accountUrl);
    PR_Free(m_outputBuffer);
    NS_IF_RELEASE(m_popServer);
    ReleaseFolderLock();
    NS_IF_RELEASE(m_folder);
    NS_IF_RELEASE(m_newMailParser);
}

nsresult
nsPop3Sink::SetUserAuthenticated(PRBool authed)
{
  m_authed = authed;
  m_popServer->SetAuthenticated(authed);
  if (authed)
  {
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_popServer);
    if (!server) 
      return NS_ERROR_UNEXPECTED;
    return server->StorePassword();
  }
  return NS_OK;
}

nsresult
nsPop3Sink::GetUserAuthenticated(PRBool* authed)
{
  return m_popServer->GetAuthenticated(authed);
}

nsresult
nsPop3Sink::SetSenderAuthedFlag(void* closure, PRBool authed)
{
  m_authed = authed;
  return NS_OK;   
}

nsresult 
nsPop3Sink::SetMailAccountURL(const char* urlString)
{
  if (urlString)
  {
    PR_Free(m_accountUrl);
    m_accountUrl = PL_strdup(urlString);
  }

  return NS_OK;
}

nsresult
nsPop3Sink::GetMailAccountURL(char* *urlString)
{
  NS_ASSERTION(urlString, "null getter in getMailAccountURL");
  if (!urlString) return NS_ERROR_NULL_POINTER;

  *urlString = nsCRT::strdup(m_accountUrl);
  return NS_OK;
}

nsresult 
nsPop3Sink::BeginMailDelivery(PRBool uidlDownload, nsIMsgWindow *aMsgWindow, PRBool* aBool)
{
#ifdef DEBUG
    m_fileCounter++;
#endif

    nsresult rv;
    
    nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_popServer);
    if (!server) 
      return NS_ERROR_UNEXPECTED;

    nsFileSpec fileSpec;
    // ### if we're doing a UIDL, then the fileSpec needs to be for the current folder

    PRBool isLocked;
    nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIPop3Sink*, this));
    m_folder->GetLocked(&isLocked);
    if(!isLocked)
      m_folder->AcquireSemaphore(supports);
    else
      return NS_MSG_FOLDER_BUSY;

    if (uidlDownload)
    {
      nsCOMPtr<nsIFileSpec> path;
      m_folder->GetPath(getter_AddRefs(path));
      path->GetFileSpec(&fileSpec);
    }
    else
    {
      nsCOMPtr<nsIFileSpec> mailDirectory;
      rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
      if (NS_FAILED(rv)) return rv;
    
      mailDirectory->GetFileSpec(&fileSpec);
      fileSpec += "Inbox";
    }
    m_outFileStream = new nsIOFileStream(fileSpec /*, PR_CREATE_FILE */);
    
    // The following (!m_outFileStream etc) was added to make sure that we don't write somewhere 
    // where for some reason or another we can't write too and lose the messages
    // See bug 62480
    if (!m_outFileStream)
        return NS_ERROR_OUT_OF_MEMORY;
 
    m_outFileStream->seek(fileSpec.GetFileSize());
 
    if (!m_outFileStream->is_open())
        return NS_ERROR_FAILURE;
    
    // create a new mail parser
    m_newMailParser = new nsParseNewMailState;
    NS_IF_ADDREF(m_newMailParser);
    if (m_newMailParser == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;

    m_folder->GetNumNewMessages(PR_FALSE, &m_numNewMessagesInFolder);
    nsCOMPtr <nsIMsgFolder> serverFolder;
    rv = GetServerFolder(getter_AddRefs(serverFolder));
    if (NS_FAILED(rv)) return rv;

    rv = m_newMailParser->Init(serverFolder, m_folder, fileSpec, m_outFileStream, aMsgWindow);
	// if we failed to initialize the parser, then just don't use it!!!
	// we can still continue without one...

    if (NS_FAILED(rv))
    {
      NS_IF_RELEASE(m_newMailParser);
      rv = NS_OK;
    }
    else
    {
      // Share the inbox fileStream so that moz-status-line flags can be set in the Inbox 
      m_newMailParser->SetDBFolderStream(m_outFileStream); 
    }

    if (uidlDownload && m_newMailParser)
        m_newMailParser->DisableFilters();

#ifdef DEBUG
    printf("Begin mail message delivery.\n");
#endif 
    if (aBool)
        *aBool = PR_TRUE;
    return NS_OK;
}

nsresult
nsPop3Sink::EndMailDelivery()
{
  if (m_newMailParser)
  {
    if (m_outFileStream)
      m_outFileStream->flush();	// try this.
    m_newMailParser->OnStopRequest(nsnull, nsnull, NS_OK);
    m_newMailParser->SetDBFolderStream(nsnull); // stream is going away
  }
  if (m_outFileStream)
  {
    m_outFileStream->close();
    delete m_outFileStream;
    m_outFileStream = 0;
  }

  // tell the parser to mark the db valid *after* closing the mailbox.
  if (m_newMailParser)
    m_newMailParser->UpdateDBFolderInfo();

  nsresult rv = ReleaseFolderLock();
  NS_ASSERTION(NS_SUCCEEDED(rv),"folder lock not released successfully");

  PRBool filtersRun;
  m_folder->CallFilterPlugins(nsnull, &filtersRun); // ??? do we need msgWindow?
  PRInt32 numNewMessagesInFolder;
  // if filters have marked msgs read or deleted, the num new messages count  
  // will go negative by the number of messages marked read or deleted,
  // so if we add that number to the number of msgs downloaded, that will give
  // us the number of actual new messages.
  m_folder->GetNumNewMessages(PR_FALSE, &numNewMessagesInFolder);
  m_numNewMessages -= (m_numNewMessagesInFolder  - numNewMessagesInFolder);
  m_folder->SetNumNewMessages(m_numNewMessages); // we'll adjust this for spam later
  if (!filtersRun && m_numNewMessages > 0)
    m_folder->SetBiffState(m_biffState);

  // note that size on disk has possibly changed.
  nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(m_folder);
  if (localFolder)
    (void) localFolder->RefreshSizeOnDisk();
  nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_popServer);
  if (server) {
    nsCOMPtr <nsIMsgFilterList> filterList;
    rv = server->GetFilterList(nsnull, getter_AddRefs(filterList)); 
    NS_ENSURE_SUCCESS(rv,rv);

    if (filterList)
      (void) filterList->FlushLogIfNecessary();
  }

  // fix for bug #161999
  // we should update the summary totals for the folder (inbox)
  // in case it's not the open folder
  m_folder->UpdateSummaryTotals(PR_TRUE);
 
  // check if the folder open in this window is not the current folder, and if it has new
  // message, in which case we need to try to run the filter plugin.
  if (m_newMailParser)
  {
    nsCOMPtr <nsIMsgWindow> msgWindow;
    m_newMailParser->GetMsgWindow(getter_AddRefs(msgWindow));
    // this breaks down if it's biff downloading new mail because
    // there's no msgWindow...
    if (msgWindow)
    {
      nsCOMPtr <nsIMsgFolder> openFolder;
      (void) msgWindow->GetOpenFolder(getter_AddRefs(openFolder));
      if (openFolder && openFolder != m_folder)
      {
        // only call filter plugins if folder is a local folder, because only
        // local folders get messages filtered into them synchronously by pop3.
        nsCOMPtr<nsIMsgLocalMailFolder> localFolder = do_QueryInterface(openFolder);
        if (localFolder)
        {
          PRBool hasNew, isLocked;
          (void) openFolder->GetHasNewMessages(&hasNew);
          if (hasNew)
          {
            // if the open folder is locked, we shouldn't run the spam filters
            // on it because someone is using the folder. see 218433.
            // Ideally, the filter plugin code would try to grab the folder lock
            // and hold onto it until done, but that's more difficult and I think
            // this will actually fix the problem.
            openFolder->GetLocked(&isLocked);
            if(!isLocked)
              openFolder->CallFilterPlugins(nsnull, &filtersRun);
          }
        }
      }
    }
  }
#ifdef DEBUG
  printf("End mail message delivery.\n");
#endif 
  return NS_OK;
}

nsresult 
nsPop3Sink::ReleaseFolderLock()
{
  nsresult result = NS_OK;
  if (!m_folder) 
    return result;
  PRBool haveSemaphore;
  nsCOMPtr <nsISupports> supports = do_QueryInterface(NS_STATIC_CAST(nsIPop3Sink*, this));
  result = m_folder->TestSemaphore(supports, &haveSemaphore);
  if(NS_SUCCEEDED(result) && haveSemaphore)
    result = m_folder->ReleaseSemaphore(supports);
  return result;
}

nsresult 
nsPop3Sink::AbortMailDelivery()
{
  if (m_newMailParser)
    m_newMailParser->SetDBFolderStream(nsnull); //stream is going away

  if (m_outFileStream)
  {
    if (m_outFileStream->is_open())
      m_outFileStream->close();
    delete m_outFileStream;
    m_outFileStream = 0;
  }

  /* tell the parser to mark the db valid *after* closing the mailbox.
  we have truncated the inbox, so berkeley mailbox and msf file are in sync*/
  if (m_newMailParser)
    m_newMailParser->UpdateDBFolderInfo();
  nsresult rv = ReleaseFolderLock();
  NS_ASSERTION(NS_SUCCEEDED(rv),"folder lock not released successfully");

#ifdef DEBUG
    printf("Abort mail message delivery.\n");
#endif 
  return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::IncorporateBegin(const char* uidlString,
                             nsIURI* aURL,
                             PRUint32 flags,
                             void** closure)
{
#ifdef DEBUG
    printf("Incorporate message begin:\n");
    if (uidlString)
        printf("uidl string: %s\n", uidlString);
#endif 
    if (closure)
        *closure = (void*) this;
    
    m_msgOffset = m_outFileStream->tell();
    char *dummyEnvelope = GetDummyEnvelope();
    
    nsresult rv = WriteLineToMailbox(dummyEnvelope);
    if (NS_FAILED(rv)) return rv;
    if (uidlString)
    {
        nsCAutoString uidlCString("X-UIDL: ");
        uidlCString += uidlString;
        uidlCString += MSG_LINEBREAK;
        rv = WriteLineToMailbox(NS_CONST_CAST(char*, uidlCString.get()));
        if (NS_FAILED(rv)) return rv;
    }
    // WriteLineToMailbox("X-Mozilla-Status: 8000" MSG_LINEBREAK);
    char *statusLine = PR_smprintf(X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, flags);
    rv = WriteLineToMailbox(statusLine);
    if (NS_FAILED(rv)) return rv;
    rv = WriteLineToMailbox("X-Mozilla-Status2: 00000000" MSG_LINEBREAK);
    if (NS_FAILED(rv)) return rv;
    PR_smprintf_free(statusLine);
    return NS_OK;
}

nsresult
nsPop3Sink::SetPopServer(nsIPop3IncomingServer *server)
{
  NS_IF_RELEASE(m_popServer);
  m_popServer=server;
  NS_ADDREF(m_popServer);
  
  return NS_OK;
}

nsresult
nsPop3Sink::GetPopServer(nsIPop3IncomingServer* *server)
{
    if (!server) return NS_ERROR_NULL_POINTER;
    *server = m_popServer;
    if (*server) NS_ADDREF(*server);
    return NS_OK;
}

nsresult nsPop3Sink::GetFolder(nsIMsgFolder * *folder)
{
  NS_ENSURE_ARG_POINTER(folder);
  NS_IF_ADDREF(*folder = m_folder);
  return NS_OK;
}

nsresult nsPop3Sink::SetFolder(nsIMsgFolder * folder)
{
  NS_IF_RELEASE(m_folder);
  m_folder=folder;
  NS_IF_ADDREF(m_folder);
  
  return NS_OK;
}

nsresult
nsPop3Sink::GetServerFolder(nsIMsgFolder **aFolder)
{
  if (!aFolder) 
    return NS_ERROR_NULL_POINTER;
  if (m_popServer)
  {
    nsCOMPtr <nsIMsgIncomingServer> incomingServer = do_QueryInterface(m_popServer);
    if (incomingServer)
      return incomingServer->GetRootFolder(aFolder);
  }
  *aFolder = nsnull;
  return NS_ERROR_NULL_POINTER;
}

char*
nsPop3Sink::GetDummyEnvelope(void)
{
  static char result[75];
  char *ct;
  time_t now = time ((time_t *) 0);
#if defined (XP_WIN)
  if (now < 0 || now > 0x7FFFFFFF)
	  now = 0x7FFFFFFF;
#endif
  ct = ctime(&now);
  PR_ASSERT(ct[24] == nsCRT::CR || ct[24] == nsCRT::LF);
  ct[24] = 0;
  /* This value must be in ctime() format, with English abbreviations.
	 strftime("... %c ...") is no good, because it is localized. */
  PL_strcpy(result, "From - ");
  PL_strcpy(result + 7, ct);
  PL_strcpy(result + 7 + 24, MSG_LINEBREAK);
  return result;
}

nsresult
nsPop3Sink::IncorporateWrite(const char* block,
                             PRInt32 length)
{
  PRInt32 blockOffset = 0;
  if (!strncmp(block, "From ", 5))
  {
    length++;
    blockOffset = 1;
  }
  if (!m_outputBuffer || length > m_outputBufferSize)
  {
    if (!m_outputBuffer)
      m_outputBuffer = (char*) PR_MALLOC(length+1);
    else
      m_outputBuffer = (char*) PR_REALLOC(m_outputBuffer, length+1);
    
    m_outputBufferSize = length;
  }
  if (m_outputBuffer)
  {
    if (blockOffset == 1)
      *m_outputBuffer = '>';
    memcpy(m_outputBuffer + blockOffset, block, length - blockOffset);
    *(m_outputBuffer + length) = 0;
    nsresult rv = WriteLineToMailbox (m_outputBuffer);
    if (NS_FAILED(rv)) return rv;
  }
  return NS_OK;
}

nsresult nsPop3Sink::WriteLineToMailbox(char *buffer)
{
  
  if (buffer)
  {
    PRInt32 bufferLen = PL_strlen(buffer);
    if (m_newMailParser)
      m_newMailParser->HandleLine(buffer, bufferLen);
    // The following (!m_outFileStream etc) was added to make sure that we don't write somewhere 
    // where for some reason or another we can't write to and lose the messages
    // See bug 62480
    if (!m_outFileStream)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ASSERTION(m_outFileStream->eof(), "we are not writing to end-of-file");
    
    if (!m_outFileStream->eof())
      m_outFileStream->seek(PR_SEEK_END, 0);
    PRInt32 bytes = m_outFileStream->write(buffer,bufferLen);
    if (bytes != bufferLen) return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::IncorporateComplete(nsIMsgWindow *aMsgWindow)
{
  if (m_buildMessageUri && m_baseMessageUri)
  {
      PRUint32 msgKey;
      m_newMailParser->GetEnvelopePos(&msgKey);
      m_messageUri.SetLength(0);
      nsBuildLocalMessageURI(m_baseMessageUri, msgKey, m_messageUri);
  }

  nsresult rv = WriteLineToMailbox(MSG_LINEBREAK);
  if (NS_FAILED(rv)) return rv;
  rv = m_outFileStream->flush();   //to make sure the message is written to the disk
  if (NS_FAILED(rv)) return rv;
  NS_ASSERTION(m_newMailParser, "could not get m_newMailParser");
  if (m_newMailParser)
    m_newMailParser->PublishMsgHeader(aMsgWindow); 

#ifdef DEBUG
  printf("Incorporate message complete.\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::IncorporateAbort(PRBool uidlDownload)
{
  nsresult rv;
  rv = m_outFileStream->close();   //need to close so that the file can be truncated.
  NS_ENSURE_SUCCESS(rv,rv);
  if (m_msgOffset >= 0)
  {
     nsCOMPtr<nsIMsgIncomingServer> server = do_QueryInterface(m_popServer);
     NS_ASSERTION(server, "Could not get the pop server !!");
     nsCOMPtr<nsIFileSpec> mailDirectory;
     if (uidlDownload)
        m_folder->GetPath(getter_AddRefs(mailDirectory));
     else
     {
       rv = server->GetLocalPath(getter_AddRefs(mailDirectory));
       NS_ENSURE_SUCCESS(rv,rv);
       rv = mailDirectory->AppendRelativeUnixPath("Inbox");
       NS_ENSURE_SUCCESS(rv,rv);
     }
     rv = mailDirectory->Truncate(m_msgOffset);
     NS_ENSURE_SUCCESS(rv,rv);
  }
#ifdef DEBUG
    printf("Incorporate message abort.\n");
#endif 
    return rv;
}

nsresult
nsPop3Sink::BiffGetNewMail()
{
#ifdef DEBUG
    printf("Biff get new mail.\n");
#endif 
    return NS_OK;
}

nsresult
nsPop3Sink::SetBiffStateAndUpdateFE(PRUint32 aBiffState, PRInt32 numNewMessages, PRBool notify)
{
  m_biffState = aBiffState;
  if (notify && m_folder && numNewMessages > 0 && numNewMessages != m_numNewMessages 
      && aBiffState == nsIMsgFolder::nsMsgBiffState_NewMail)
  {
    m_folder->SetNumNewMessages(numNewMessages);
    m_folder->SetBiffState(aBiffState);
  }
  m_numNewMessages = numNewMessages;

  return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::GetBuildMessageUri(PRBool *bVal)
{
    if (!bVal)
        return NS_ERROR_NULL_POINTER;
    *bVal = m_buildMessageUri;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::SetBuildMessageUri(PRBool bVal)
{
    m_buildMessageUri = bVal;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::GetMessageUri(char **messageUri)
{
    if (!messageUri || m_messageUri.Length() <= 0) 
        return NS_ERROR_NULL_POINTER;
    *messageUri = ToNewCString(m_messageUri);
    return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::SetMessageUri(const char *messageUri)
{
    if (!messageUri)
        return NS_ERROR_NULL_POINTER;
    m_messageUri = messageUri;
    return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::GetBaseMessageUri(char **baseMessageUri)
{
    if (!baseMessageUri || !m_baseMessageUri)
        return NS_ERROR_NULL_POINTER;
    *baseMessageUri = nsCRT::strdup((const char *) m_baseMessageUri);
    return NS_OK;
}

NS_IMETHODIMP
nsPop3Sink::SetBaseMessageUri(const char *baseMessageUri)
{
    if (!baseMessageUri)
        return NS_ERROR_NULL_POINTER;
    m_baseMessageUri.Adopt(nsCRT::strdup(baseMessageUri));
    return NS_OK;
}
