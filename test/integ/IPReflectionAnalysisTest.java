/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

package com.facebook.redextest;

import java.lang.reflect.Method;

public class IPReflectionAnalysisTest {
  static Class reflClass() throws Exception {
    return Class.forName("com.facebook.redextest.IPReflectionAnalysisTest");
  }

  static Method reflMethod() throws Exception {
    return reflClass().getDeclaredMethod("abc");
  }

  static Method callsReflMethod() throws Exception {
    return reflMethod();
  }

  static Class callsReflClass() throws Exception {
    return reflClass();
  }

  static Method reflMethodWithCallsReflClass() throws Exception {
    return callsReflClass().getDeclaredMethod("abc");
  }

  static Method reflMethodWithInputClass(Class c) throws Exception {
    return c.getDeclaredMethod("abc");
  }

  static Method callsReflMethodWithInputClass() throws Exception {
    return reflMethodWithInputClass(reflClass());
  }

  static Class reflClassWithInputString(String name) throws Exception {
    return Class.forName(name);
  }

  static Class callsReflClassWithInputString() throws Exception {
    return reflClassWithInputString("com.facebook.redextest.IPReflectionAnalysisTest");
  }

  static Method reflMethodWithInputString(String clsName, String methodName) throws Exception {
    Class c = Class.forName(clsName);
    return c.getDeclaredMethod(methodName);
  }

  static Method callsReflMethodWithInputString() throws Exception {
    return reflMethodWithInputString("com.facebook.redextest.IPReflectionAnalysisTest", "abc");
  }

  static String getClassName() {
    return "com.facebook.redextest.IPReflectionAnalysisTest";
  }

  static Class reflClassWithCallGetClassName() throws Exception {
    return Class.forName(getClassName());
  }
}
