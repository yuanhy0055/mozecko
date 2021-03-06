/* -*- Mode: C; tab-width: 8 -*-*/
/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */


/*
 * This file will contain all routines needed by a client that has 
 * to parse a CMMFCertRepContent structure and retirieve the appropriate
 * data.
 */

#include "cmmf.h"
#include "cmmfi.h"
#include "crmf.h"
#include "crmfi.h"
#include "secitem.h"
#include "secder.h"
#include "secasn1.h"

CMMFCertRepContent*
CMMF_CreateCertRepContentFromDER(CERTCertDBHandle *db, const char *buf, 
				 long len)
{
    PRArenaPool        *poolp;
    CMMFCertRepContent *certRepContent;
    SECStatus           rv;
    int                 i;

    poolp = PORT_NewArena(CRMF_DEFAULT_ARENA_SIZE);
    if (poolp == NULL) {
        return NULL;
    }
    certRepContent = PORT_ArenaZNew(poolp, CMMFCertRepContent);
    if (certRepContent == NULL) {
        goto loser;
    }
    certRepContent->poolp = poolp;
    rv = SEC_ASN1Decode(poolp, certRepContent, CMMFCertRepContentTemplate,
			buf, len);
    if (rv != SECSuccess) {
        goto loser;
    }
    if (certRepContent->response != NULL) {
        for (i=0; certRepContent->response[i] != NULL; i++) {
	    rv = cmmf_decode_process_cert_response(poolp, db,
					       certRepContent->response[i]);
	    if (rv != SECSuccess) {
	        goto loser;
	    }
	}
    }
    certRepContent->isDecoded = PR_TRUE;
    return certRepContent;
 loser:
    PORT_FreeArena(poolp, PR_FALSE);
    return NULL;
}

long
CMMF_CertResponseGetCertReqId(CMMFCertResponse *inCertResp)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL) {
        return -1;
    }
    return DER_GetInteger(&inCertResp->certReqId);
}

PRBool
cmmf_CertRepContentIsIndexValid(CMMFCertRepContent *inCertRepContent,
                                int                 inIndex)
{
    int numResponses;

    PORT_Assert(inCertRepContent != NULL);
    numResponses = CMMF_CertRepContentGetNumResponses(inCertRepContent);
    return (PRBool)(inIndex >= 0 && inIndex < numResponses);
}

CMMFCertResponse*
CMMF_CertRepContentGetResponseAtIndex(CMMFCertRepContent *inCertRepContent,
				      int                 inIndex)
{
    CMMFCertResponse *certResponse;
    SECStatus         rv;

    PORT_Assert(inCertRepContent != NULL &&
		cmmf_CertRepContentIsIndexValid(inCertRepContent, inIndex));
    if (inCertRepContent == NULL ||
	!cmmf_CertRepContentIsIndexValid(inCertRepContent, inIndex)) {
        return NULL;
    }
    certResponse = PORT_ZNew(CMMFCertResponse);
    rv = cmmf_CopyCertResponse(NULL, certResponse, 
			       inCertRepContent->response[inIndex]);
    if (rv != SECSuccess) {
        CMMF_DestroyCertResponse(certResponse);
	certResponse = NULL;
    }
    return certResponse;
}

CMMFPKIStatus
CMMF_CertResponseGetPKIStatusInfoStatus(CMMFCertResponse *inCertResp)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL) {
        return cmmfNoPKIStatus;
    }
    return cmmf_PKIStatusInfoGetStatus(&inCertResp->status);
}

CERTCertificate*
CMMF_CertResponseGetCertificate(CMMFCertResponse *inCertResp,
				CERTCertDBHandle *inCertdb)
{
    PORT_Assert(inCertResp != NULL);
    if (inCertResp == NULL || inCertResp->certifiedKeyPair == NULL) {
        return NULL;
    }
    
    return cmmf_CertOrEncCertGetCertificate
                                (&inCertResp->certifiedKeyPair->certOrEncCert,
				 inCertdb);
				   
}

CERTCertList*
CMMF_CertRepContentGetCAPubs (CMMFCertRepContent *inCertRepContent)
{
    PORT_Assert (inCertRepContent != NULL);
    if (inCertRepContent == NULL || inCertRepContent->caPubs == NULL) {
        return NULL;
    }
    return cmmf_MakeCertList(inCertRepContent->caPubs);
}

