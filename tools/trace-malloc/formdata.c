/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is formdata.c code, released
 * May 9, 2002.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2002 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *    Garrett Arch Blythe, 09-May-2002
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 */

/*
**  formdata.c
**
**  Play utility to parse up form get data into name value pairs.
*/

#include "formdata.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static void unhexcape(char* inPlace)
/*
**  Real low tech unhexcaper....
**
**  inPlace     string to decode, in place as it were.
*/
{
    if(NULL != inPlace)
    {
        int index1 = 0;
        int index2 = 0;
        int theLen = strlen(inPlace);
        
        for(; index1 <= theLen; index1++)
        {
            if('%' == inPlace[index1] && '\0' != inPlace[index1 + 1] && '\0' != inPlace[index1 + 2])
            {
                int unhex = 0;
                
                if('9' >= inPlace[index1 + 1])
                {
                    unhex |= ((inPlace[index1 + 1] - '0') << 4);
                }
                else
                {
                    unhex |= ((toupper(inPlace[index1 + 1]) - 'A' + 10) << 4);
                }
                
                if('9' >= inPlace[index1 + 2])
                {
                    unhex |= (inPlace[index1 + 2] - '0');
                }
                else
                {
                    unhex |= (toupper(inPlace[index1 + 2]) - 'A' + 10);
                }
                
                index1 += 2;
                inPlace[index1] = unhex;
            }
            
            inPlace[index2++] = inPlace[index1];
        }
    }
}


FormData* FormData_Create(const char* inFormData)
{
    FormData* retval = NULL;

    if(NULL != inFormData)
    {
        FormData* container = NULL;

        /*
        **  Allocate form data container.
        */
        container = (FormData*)calloc(1, sizeof(FormData));
        if(NULL != container)
        {
            /*
            **  Dup the incoming form data.
            */
            container->mStorage = strdup(inFormData);
            if(NULL != container->mStorage)
            {
                char* traverse = NULL;
                unsigned nvpairs = 1;
                unsigned storeLen = 0;

                /*
                **  Count the number of pairs we are going to have.
                **  We do this by counting '&' + 1.
                */
                for(traverse = container->mStorage; '\0' != *traverse; traverse++)
                {
                    if('&' == *traverse)
                    {
                        nvpairs++;
                    }
                }
                storeLen = (unsigned)(traverse - container->mStorage);

                /*
                **  Allocate space for our names and values.
                */
                container->mNArray = (char**)calloc(nvpairs * 2, sizeof(char*));
                if(NULL != container->mNArray)
                {
                    char* amp = NULL;
                    char* equ = NULL;

                    container->mVArray = &container->mNArray[nvpairs];

                    /*
                    **  Go back over the storage.
                    **  Fill in the names and values as we go.
                    **  Terminate on dividing '=' and '&' characters.
                    **  Increase the count of items as we go.
                    */
                    for(traverse = container->mStorage; NULL != traverse; container->mNVCount++)
                    {
                        container->mNArray[container->mNVCount] = traverse;

                        amp = strchr(traverse, '&');
                        equ = strchr(traverse, '=');
                        traverse = NULL;

                        if(NULL != equ && (NULL == amp || equ < amp))
                        {
                            *equ++ = '\0';

                            container->mVArray[container->mNVCount] = equ;
                        }
                        else
                        {
                            container->mVArray[container->mNVCount] = (container->mStorage + storeLen);
                        }

                        if(NULL != amp)
                        {
                            *amp++ = '\0';

                            traverse = amp;
                        }

                        unhexcape(container->mNArray[container->mNVCount]);
                        unhexcape(container->mVArray[container->mNVCount]);
                    }

                    retval = container;
                }
            }
        }

        /*
        **  If we failed, cleanup.
        */
        if(NULL == retval)
        {
            FormData_Destroy(container);
        }
    }

    return retval;
}


void FormData_Destroy(FormData* inDestroy)
{
    if(NULL != inDestroy)
    {
        unsigned traverse = 0;

        for(traverse = 0; traverse < inDestroy->mNVCount; traverse++)
        {
            if(NULL != inDestroy->mNArray)
            {
                inDestroy->mNArray[traverse] = NULL;
            }
            if(NULL != inDestroy->mVArray)
            {
                inDestroy->mVArray[traverse] = NULL;
            }
        }
        inDestroy->mNVCount = 0;

        if(NULL != inDestroy->mStorage)
        {
            free(inDestroy->mStorage);
            inDestroy->mStorage = NULL;
        }

        if(NULL != inDestroy->mNArray)
        {
            free(inDestroy->mNArray);
            inDestroy->mNArray = NULL;
            inDestroy->mVArray = NULL;
        }

        free(inDestroy);
        inDestroy = NULL;
    }
}
