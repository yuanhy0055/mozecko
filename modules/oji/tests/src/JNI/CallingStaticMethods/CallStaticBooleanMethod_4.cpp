/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "JNIEnvTests.h"
#include "CallingStaticMethods.h"

JNI_OJIAPITest(JNIEnv_CallStaticBooleanMethod_4)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetStaticMethodID_METHOD("Test1", "Test1_method_bool_static", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z");
  jboolean value = env->CallStaticBooleanMethod(clazz, MethodID, (jboolean)JNI_FALSE, (jbyte)MIN_JBYTE, (jchar)0, (jshort)1, (jint)123, (jlong)20, (jfloat)10., (jdouble)100, (jobject)NULL, (jobject)NULL);
  if(value == JNI_FALSE){
     return TestResult::PASS("CallStaticBooleanMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z) return correct value");
  }else{
     return TestResult::FAIL("CallStaticBooleanMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z) return incorrect value");
  }

}

/*
#include "JNIEnvTests.h"
#include "CallingStaticMethods.h"

JNI_OJIAPITest(JNIEnv_CallStaticBooleanMethod_4)
{
  GET_JNI_FOR_TEST

  IMPLEMENT_GetMethodID_METHOD("Test1", "Test1_method_bool_static", "(ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z");
  jboolean value = env->CallStaticBooleanMethod(clazz, MethodID, (jboolean)JNI_FALSE, (jbyte)MIN_JBYTE, (jchar)0, (jshort)1, (jint)123, (jlong)20, (jfloat)10., (jdouble)100, (jobject)NULL, (jobject)NULL);
  if(value == JNI_FALSE){
     return TestResult::PASS("CallStaticBooleanMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z) return correct value");
  }else{
     return TestResult::FAIL("CallStaticBooleanMethod for public not inherited method (sig = (ZBCSIJFDLjava/lang/String;[Ljava/lang/String;)Z) return incorrect value");
  }

}
*/
