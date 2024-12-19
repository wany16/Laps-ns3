/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

// Include a header file from your module to test.
#include "ns3/ipv4-deflow-routing.h"

// An essential include is test.h
#include "ns3/test.h"

// Do not put your test classes in namespace ns3.  You may find it useful
// to use the using directive to access the ns3 namespace directly
using namespace ns3;

// This is an example TestCase.
class Ipv4DeflowRoutingTestCase1 : public TestCase
{
public:
  Ipv4DeflowRoutingTestCase1 ();
  virtual ~Ipv4DeflowRoutingTestCase1 ();

private:
  virtual void DoRun (void);
};

// Add some help text to this case to describe what it is intended to test
Ipv4DeflowRoutingTestCase1::Ipv4DeflowRoutingTestCase1 ()
  : TestCase ("Ipv4 Deflow routing test case (does nothing)")
{
}

// This destructor does nothing but we include it as a reminder that
// the test case should clean up after itself
Ipv4DeflowRoutingTestCase1::~Ipv4DeflowRoutingTestCase1 ()
{
}

//
// This method is the pure virtual method from class TestCase that every
// TestCase must implement
//
void Ipv4DeflowRoutingTestCase1::DoRun (void)
{
  // A wide variety of test macros are available in src/core/test.h
  NS_TEST_ASSERT_MSG_EQ (true, true, "true doesn't equal true for some reason");
  // Use this one for floating point comparisons
  NS_TEST_ASSERT_MSG_EQ_TOL (0.01, 0.01, 0.001, "Numbers are not equal within tolerance");
}

// The TestSuite class names the TestSuite, identifies what type of TestSuite,
// and enables the TestCases to be run.  Typically, only the constructor for
// this class must be defined
//
class Ipv4DeflowRoutingTestSuite : public TestSuite
{
public:
  Ipv4DeflowRoutingTestSuite ();
};

Ipv4DeflowRoutingTestSuite::Ipv4DeflowRoutingTestSuite ()
  : TestSuite ("Ipv4-deflow-routing", UNIT)
{
  // TestDuration for TestCase can be QUICK, EXTENSIVE or TAKES_FOREVER
  AddTestCase (new Ipv4DeflowRoutingTestCase1, TestCase::QUICK);
}

// Do not forget to allocate an instance of this TestSuite
static Ipv4DeflowRoutingTestSuite ipv4DeflowRoutingTestSuite;

