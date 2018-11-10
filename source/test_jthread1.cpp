#include "jthread.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <cassert>
#include <atomic>
using namespace::std::literals;

//------------------------------------------------------

void testJThreadWithout()
{
  // test the basic jthread API (not taking stop_token arg)
  std::cout << "*** start testJThreadWithout()" << std::endl;

  assert(std::jthread::hardware_concurrency() == std::thread::hardware_concurrency()); 
  std::stop_token stoken;
  assert(!stoken.stop_done_or_possible());
  {
    std::jthread::id t1ID{std::this_thread::get_id()};
    std::atomic<bool> t1AllSet{false};
    std::jthread t1([&t1ID, &t1AllSet] {
                   // check some values of the started thread:
                   t1ID = std::this_thread::get_id();
                   t1AllSet.store(true);
                   // wait until loop is done (no interrupt checked)
                   for (int c='9'; c>='0'; --c) {
                      std::this_thread::sleep_for(222ms);
                      std::cout.put(static_cast<char>(c)).flush();
                   }
                   std::cout << "END t1" << std::endl;
                 });
    // wait until t1 has set all initial values:
    for (int i=0; !t1AllSet.load(); ++i) {
      std::this_thread::sleep_for(10ms);
      std::cout.put('o').flush();
    }
    // and check all values:
    assert(t1.joinable());
    assert(t1ID == t1.get_id());
    stoken = t1.get_stop_source().get_token();
    assert(!stoken.stop_signaled());
  } // leave scope of t1 without join() or detach() (signals cancellation)
  assert(stoken.stop_signaled());
  std::cout << "\n*** OK" << std::endl;
}

//------------------------------------------------------

void testThreadWithToken()
{
  // test the basic thread API (taking stop_token arg)
  std::cout << "*** start testThreadWithToken()" << std::endl;

  std::stop_source isource;
  std::stop_source origsource;
  assert(isource.is_valid());
  assert(!isource.stop_signaled());
  {
    std::jthread::id t1ID{std::this_thread::get_id()};
    std::atomic<bool> t1AllSet{false};
    std::atomic<bool> t1done{false};
    std::jthread t1([&t1ID, &t1AllSet, &t1done] (std::stop_token st) {
                       // check some values of the started thread:
                       t1ID = std::this_thread::get_id();
                       t1AllSet.store(true);
                       // wait until interrupt is signaled (due to destructor of t1):
                       for (int i=0; !st.stop_signaled(); ++i) {
                          std::this_thread::sleep_for(100ms);
                          std::cout.put('.').flush();
                       }
                       t1done.store(true);
                       std::cout << "END t1" << std::endl;
                     },
                     isource.get_token());
    // wait until t1 has set all initial values:
    for (int i=0; !t1AllSet.load(); ++i) {
      std::this_thread::sleep_for(10ms);
      std::cout.put('o').flush();
    }
    // and check all values:
    assert(t1.joinable());
    assert(t1ID == t1.get_id());

    std::this_thread::sleep_for(470ms);
    origsource = std::move(isource);
    isource = t1.get_stop_source();
    assert(!isource.stop_signaled());
    auto ret = isource.signal_stop();
    assert(!ret);
    ret = isource.signal_stop();
    assert(ret);
    assert(isource.stop_signaled());
    assert(!t1done.load());
    assert(!origsource.stop_signaled());

    std::this_thread::sleep_for(470ms);
    origsource.signal_stop();
  } // leave scope of t1 without join() or detach() (signals cancellation)
  assert(origsource.stop_signaled());
  assert(isource.stop_signaled());
  std::cout << "\n*** OK" << std::endl;
}

//------------------------------------------------------

void testJoin()
{
  // test jthread join()
  std::cout << "\n*** start testJoin()" << std::endl;

  std::stop_source isource;
  assert(isource.is_valid());
  {
    std::jthread t1([](std::stop_token stoken) {
                      // wait until interrupt is signaled (due to calling signal_stop() for the token):
                      for (int i=0; !stoken.stop_signaled(); ++i) {
                         std::this_thread::sleep_for(100ms);
                         std::cout.put('.').flush();
                      }
                      std::cout << "END t1" << std::endl;
                 });
    isource = t1.get_stop_source();
    // let nother thread signal cancellation after some time:
    std::jthread t2([isource] () mutable {
                     for (int i=0; i < 10; ++i) {
                       std::this_thread::sleep_for(70ms);
                       std::cout.put('x').flush();
                     }
                     // signal interrupt to other thread:
                     isource.signal_stop();
                     std::cout << "END t2" << std::endl;
                   });
    // wait for all thread to finish:
    t2.join();
    assert(!t2.joinable());
    assert(t1.joinable());
    t1.join();
    assert(!t1.joinable());
  } // leave scope of t1 without join() or detach() (signals cancellation)
  std::cout << "\n*** OK" << std::endl;
}

//------------------------------------------------------

void testDetach()
{
  // test jthread detach()
  std::cout << "\n*** start testDetach()" << std::endl;

  std::stop_source isource;
  assert(isource.is_valid());
  std::atomic<bool> t1FinallyInterrupted{false};
  {
    std::jthread t0;
    std::jthread::id t1ID{std::this_thread::get_id()};
    bool t1IsInterrupted;
    std::stop_token t1InterruptToken;
    std::atomic<bool> t1AllSet{false};
    std::jthread t1([&t1ID, &t1IsInterrupted, &t1InterruptToken, &t1AllSet, &t1FinallyInterrupted]
                    (std::stop_token stoken) {
                   // check some values of the started thread:
                   t1ID = std::this_thread::get_id();
                   t1InterruptToken = stoken;
                   t1IsInterrupted = stoken.stop_signaled();
                   assert(stoken.stop_done_or_possible());
                   assert(!stoken.stop_signaled());
                   t1AllSet.store(true);
                   // wait until interrupt is signaled (due to calling signal_stop() for the token):
                   for (int i=0; !stoken.stop_signaled(); ++i) {
                      std::this_thread::sleep_for(100ms);
                      std::cout.put('.').flush();
                   }
                   t1FinallyInterrupted.store(true);
                   std::cout << "END t1" << std::endl;
                 });
    // wait until t1 has set all initial values:
    for (int i=0; !t1AllSet.load(); ++i) {
      std::this_thread::sleep_for(10ms);
      std::cout.put('o').flush();
    }
    // and check all values:
    assert(!t0.joinable());
    //assert(std::stop_source{} == t0.get_stop_source());
    assert(t1.joinable());
    assert(t1ID == t1.get_id());
    assert(t1IsInterrupted == false);
    assert(t1InterruptToken == t1.get_stop_source().get_token());
    isource = t1.get_stop_source();
    assert(t1InterruptToken.stop_done_or_possible());
    assert(!t1InterruptToken.stop_signaled());
    // test detach():
    t1.detach();
    assert(!t1.joinable());
  } // leave scope of t1 without join() or detach() (signals cancellation)

  // finally signal cancellation:
  assert(!t1FinallyInterrupted.load());
  isource.signal_stop();
  // and check consequences:
  assert(isource.stop_signaled());
  for (int i=0; !t1FinallyInterrupted.load() && i < 100; ++i) {
    std::this_thread::sleep_for(100ms);
    std::cout.put('o').flush();
  }
  assert(t1FinallyInterrupted.load());
  std::cout << "\n*** OK" << std::endl;
}

//------------------------------------------------------

void testStdThread()
{
  // test the extended std::thread API
  std::cout << "\n*** start testStdThread()" << std::endl;
  std::thread t0;
  std::thread::id t1ID{std::this_thread::get_id()};
  std::atomic<bool> t1AllSet{false};
  std::stop_source t1ShallDie;
  std::thread t1([&t1ID, &t1AllSet, t1ShallDie = t1ShallDie.get_token()] {
                   // check some supplementary values of the started thread:
                   t1ID = std::this_thread::get_id();
                   t1AllSet.store(true);
                   // and wait until cancellation is signaled via passed token:
                   try {
                     for (int i=0; ; ++i) {
                        if (t1ShallDie.stop_signaled()) {
                          throw "interrupted";
                        }
                        std::this_thread::sleep_for(100ms);
                        std::cout.put('.').flush();
                     }
                     assert(false);
                   }
                   catch (std::exception&) { // interrupted not derived from std::exception
                     assert(false);
                   }
                   catch (const char* e) {
                     std::cout << e << std::endl;
                   }
                   catch (...) {
                     assert(false);
                   }
                   assert(t1ShallDie.stop_signaled());
                   std::cout << "END t1" << std::endl;
                 });
  // wait until t1 has set all initial values:
  for (int i=0; !t1AllSet.load(); ++i) {
    std::this_thread::sleep_for(10ms);
    std::cout.put('o').flush();
  }
  // and check all values:
  assert(t1ID == t1.get_id());
  // signal cancellation via manually installed interrupt token:
  t1ShallDie.signal_stop();
  t1.join();
  std::cout << "\n*** OK" << std::endl;
}

//------------------------------------------------------

void testTemporarilyDisableToken()
{
  // test exchanging the token to disable it temporarily
  std::cout << "*** start testTemporarilyDisableToken()" << std::endl;

  enum class State { init, loop, disabled, restored, interrupted };
  std::atomic<State> state{State::init};
  std::stop_source t1is;
  {
    std::jthread t1([&state] (std::stop_token stoken) {
                   std::cout << "- start t1" << std::endl;
                   auto actToken = stoken;
                   // just loop (no interrupt should occur):
                   state.store(State::loop); 
                   try {
                     for (int i=0; i < 10; ++i) {
                        if (actToken.stop_signaled()) {
                          throw "interrupted";
                        }
                        std::this_thread::sleep_for(100ms);
                        std::cout.put('.').flush();
                     }
                   }
                   catch (...) {
                     assert(false);
                   }
                   // temporarily disable interrupts:
                   std::stop_token interruptDisabled;
                   swap(stoken, interruptDisabled);
                   state.store(State::disabled); 
                   // loop again until interrupt signaled to original interrupt token:
                   try {
                     while (!actToken.stop_signaled()) {
                        if (stoken.stop_signaled()) {
                          throw "interrupted";
                        }
                        std::this_thread::sleep_for(100ms);
                        std::cout.put('d').flush();
                     }
                     for (int i=0; i < 10; ++i) {
                        std::this_thread::sleep_for(100ms);
                        std::cout.put('D').flush();
                     }
                   }
                   catch (...) {
                     assert(false);
                   }
                   state.store(State::restored); 
                   // enable interrupts again:
                   swap(stoken, interruptDisabled);
                   // loop again (should immediately throw):
                   assert(!interruptDisabled.stop_signaled());
                   try {
                     if (actToken.stop_signaled()) {
                       throw "interrupted";
                     }
                   }
                   catch (const char* e) {
                     std::cout.put('i').flush();
                     state.store(State::interrupted); 
                   }
                   std::cout << "\n- end t1" << std::endl;
                 });
    while (state.load() != State::disabled) {
      std::this_thread::sleep_for(100ms);
      std::cout.put('m').flush();
    }
    std::this_thread::sleep_for(500ms);
    std::cout << "\n- leave scope (should interrupt started thread)" << std::endl;
    t1is = t1.get_stop_source();
  } // leave scope of t1 without join() or detach() (signals cancellation)
  assert(t1is.stop_signaled());
  assert(state.load() == State::interrupted);
  std::cout << "\n*** OK" << std::endl;
}


//------------------------------------------------------

void testJThreadAPI()
{
  // test the basic jthread API (taking stop_token arg)
  std::cout << "*** start testJThreadAPI()" << std::endl;

  assert(std::jthread::hardware_concurrency() == std::thread::hardware_concurrency()); 
  std::stop_source isource;
  assert(isource.is_valid());
  assert(isource.get_token().stop_done_or_possible());
  std::stop_token stoken;
  assert(!stoken.stop_done_or_possible());

  // thread with no callable and invalid source:
  std::jthread t0;
  std::jthread::native_handle_type nh = t0.native_handle();
  assert((std::is_same_v<decltype(nh), std::thread::native_handle_type>)); 
  assert(!t0.joinable());
  std::stop_source isourceStolen{std::move(isource)};
  assert(!isource.is_valid());
  assert(isource == t0.get_stop_source());

  {
    std::jthread::id t1ID{std::this_thread::get_id()};
    std::stop_token t1InterruptToken;
    std::atomic<bool> t1AllSet{false};
    std::jthread t1([&t1ID, &t1InterruptToken, &t1AllSet] (std::stop_token stoken) {
                   // check some values of the started thread:
                   t1ID = std::this_thread::get_id();
                   t1InterruptToken = stoken;
                   assert(stoken.stop_done_or_possible());
                   assert(!stoken.stop_signaled());
                   t1AllSet.store(true);
                   // wait until interrupt is signaled (due to destructor of t1):
                   for (int i=0; !stoken.stop_signaled(); ++i) {
                      std::this_thread::sleep_for(100ms);
                      std::cout.put('.').flush();
                   }
                   std::cout << "END t1" << std::endl;
                 });
    // wait until t1 has set all initial values:
    for (int i=0; !t1AllSet.load(); ++i) {
      std::this_thread::sleep_for(10ms);
      std::cout.put('o').flush();
    }
    // and check all values:
    assert(t1.joinable());
    assert(t1ID == t1.get_id());
    assert(t1InterruptToken == t1.get_stop_source().get_token());
    stoken = t1.get_stop_source().get_token();
    assert(t1InterruptToken.stop_done_or_possible());
    assert(!t1InterruptToken.stop_signaled());
    // test swap():
    std::swap(t0, t1);
    assert(!t1.joinable());
    assert(std::stop_token{} == t1.get_stop_source().get_token());
    assert(t0.joinable());
    assert(t1ID == t0.get_id());
    assert(t1InterruptToken == t0.get_stop_source().get_token());
    // manual swap with move():
    auto ttmp{std::move(t0)};
    t0 = std::move(t1);
    t1 = std::move(ttmp);
    assert(!t0.joinable());
    assert(std::stop_token{} == t0.get_stop_source().get_token());
    assert(t1.joinable());
    assert(t1ID == t1.get_id());
    assert(t1InterruptToken == t1.get_stop_source().get_token());
  } // leave scope of t1 without join() or detach() (signals cancellation)
  assert(stoken.stop_signaled());
  std::cout << "\n*** OK" << std::endl;
}


//------------------------------------------------------
//------------------------------------------------------

int main()
{
  std::set_terminate([](){
                       std::cout << "ERROR: terminate() called" << std::endl;
                       assert(false);
                     });

  std::cout << "\n\n**************************\n";
  testJThreadWithout();
  std::cout << "\n\n**************************\n";
  testThreadWithToken();
  std::cout << "\n\n**************************\n";
  testJoin();
  std::cout << "\n\n**************************\n";
  testDetach();
  std::cout << "\n\n**************************\n";
  testStdThread();
  std::cout << "\n\n**************************\n";
  testTemporarilyDisableToken();
  std::cout << "\n\n**************************\n";
  testJThreadAPI();
  std::cout << "\n\n**************************\n";
}

