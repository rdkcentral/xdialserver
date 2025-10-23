/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <cstring>
#include "SecurityTokenUtil.h"
/*
 * Send thunder security token for localhost.
 * This token can be used by native applications to securely access rdkservices.
 */

using namespace std;

extern "C" {
	/*
	 * GetSecurityToken - function to obtain a token from SecurityAgent
	 *
	 * Parameters
	 *  maxLength   - holds the maximum uint8_t length of the buffer
	 *  Id          - Buffer to hold the token.
	 *
	 * Return value
	 *  < 0 - failure, absolute value returned is the length required to store the token
	 *  > 0 - success, char length of the returned token
	 *
	 * Post-condition; return value 0 should not occur
	 *
	 */
	int GetSecurityToken(unsigned short maxLength, unsigned char buffer[])
	{
		// get a localhost token
		string payload = "http://localhost";

		size_t len = payload.length();

		if(!memcpy(buffer,payload.c_str(),len))
			return -1;
		return 0;
	}
}
