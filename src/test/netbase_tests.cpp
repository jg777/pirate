// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "netbase.h"
#include "test/test_bitcoin.h"
#include "utilstrencodings.h"

#include <string>

#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(netbase_tests, BasicTestingSetup)

static CNetAddr CreateInternal(const char* host)
{
    CNetAddr addr;
    addr.SetInternal(host);
    return addr;
}

BOOST_AUTO_TEST_CASE(netbase_networks)
{
    BOOST_CHECK(ResolveIP("127.0.0.1").GetNetwork()                              == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("::1").GetNetwork()                                    == NET_UNROUTABLE);
    BOOST_CHECK(ResolveIP("8.8.8.8").GetNetwork()                                == NET_IPV4);
    BOOST_CHECK(ResolveIP("2001::8888").GetNetwork()                             == NET_IPV6);
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetNetwork() == NET_TOR);
    BOOST_CHECK(CreateInternal("foo.com").GetNetwork()                           == NET_INTERNAL);

}

BOOST_AUTO_TEST_CASE(netbase_properties)
{

    BOOST_CHECK(ResolveIP("127.0.0.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::FFFF:192.168.1.1").IsIPv4());
    BOOST_CHECK(ResolveIP("::1").IsIPv6());
    BOOST_CHECK(ResolveIP("10.0.0.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("192.168.1.1").IsRFC1918());
    BOOST_CHECK(ResolveIP("172.31.255.255").IsRFC1918());
    BOOST_CHECK(ResolveIP("2001:0DB8::").IsRFC3849());
    BOOST_CHECK(ResolveIP("169.254.1.1").IsRFC3927());
    BOOST_CHECK(ResolveIP("2002::1").IsRFC3964());
    BOOST_CHECK(ResolveIP("FC00::").IsRFC4193());
    BOOST_CHECK(ResolveIP("2001::2").IsRFC4380());
    BOOST_CHECK(ResolveIP("2001:10::").IsRFC4843());
    BOOST_CHECK(ResolveIP("2001:20::").IsRFC7343());
    BOOST_CHECK(ResolveIP("FE80::").IsRFC4862());
    BOOST_CHECK(ResolveIP("64:FF9B::").IsRFC6052());
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").IsTor());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsLocal());
    BOOST_CHECK(ResolveIP("::1").IsLocal());
    BOOST_CHECK(ResolveIP("8.8.8.8").IsRoutable());
    BOOST_CHECK(ResolveIP("2001::1").IsRoutable());
    BOOST_CHECK(ResolveIP("127.0.0.1").IsValid());
    BOOST_CHECK(CreateInternal("FD6B:88C0:8724:edb1:8e4:3588:e546:35ca").IsInternal());
    BOOST_CHECK(CreateInternal("bar.com").IsInternal());

}

bool static TestSplitHost(string test, string host, int port)
{
    string hostOut;
    int portOut = -1;
    SplitHostPort(test, portOut, hostOut);
    return hostOut == host && port == portOut;
}

BOOST_AUTO_TEST_CASE(netbase_splithost)
{
    BOOST_CHECK(TestSplitHost("www.bitcoin.org", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]", "www.bitcoin.org", -1));
    BOOST_CHECK(TestSplitHost("www.bitcoin.org:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("[www.bitcoin.org]:80", "www.bitcoin.org", 80));
    BOOST_CHECK(TestSplitHost("127.0.0.1", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("127.0.0.1:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]", "127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[127.0.0.1]:8333", "127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("::ffff:127.0.0.1", "::ffff:127.0.0.1", -1));
    BOOST_CHECK(TestSplitHost("[::ffff:127.0.0.1]:8333", "::ffff:127.0.0.1", 8333));
    BOOST_CHECK(TestSplitHost("[::]:8333", "::", 8333));
    BOOST_CHECK(TestSplitHost("::8333", "::8333", -1));
    BOOST_CHECK(TestSplitHost(":8333", "", 8333));
    BOOST_CHECK(TestSplitHost("[]:8333", "", 8333));
    BOOST_CHECK(TestSplitHost("", "", -1));
}

bool static TestParse(string src, string canon)
{
    CService addr(LookupNumeric(src.c_str(), 65535));
    return canon == addr.ToString();
}

BOOST_AUTO_TEST_CASE(netbase_lookupnumeric)
{
    BOOST_CHECK(TestParse("127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("127.0.0.1:8333", "127.0.0.1:8333"));
    BOOST_CHECK(TestParse("::ffff:127.0.0.1", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse("::", "[::]:65535"));
    BOOST_CHECK(TestParse("[::]:8333", "[::]:8333"));
    BOOST_CHECK(TestParse("[127.0.0.1]", "127.0.0.1:65535"));
    BOOST_CHECK(TestParse(":::", "[::]:0"));

    // verify that an internal address fails to resolve
    BOOST_CHECK(TestParse("[fd6b:88c0:8724:1:2:3:4:5]", "[::]:0"));
    // and that a one-off resolves correctly
    BOOST_CHECK(TestParse("[fd6c:88c0:8724:1:2:3:4:5]", "[fd6c:88c0:8724:1:2:3:4:5]:65535"));
}

BOOST_AUTO_TEST_CASE(onioncat_test)
{
    // values from https://web.archive.org/web/20121122003543/http://www.cypherpunk.at/onioncat/wiki/OnionCat
    CNetAddr addr1("5wyqrzbvrdsumnok.onion");
    CNetAddr addr2("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca");
    BOOST_CHECK(addr1 == addr2);
    BOOST_CHECK(addr1.IsTor());
    BOOST_CHECK(addr1.ToStringIP() == "5wyqrzbvrdsumnok.onion");
    BOOST_CHECK(addr1.IsRoutable());
}

BOOST_AUTO_TEST_CASE(subnet_test)
{
    BOOST_CHECK(CSubNet("1.2.3.0/24") == CSubNet("1.2.3.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24") != CSubNet("1.2.4.0/255.255.255.0"));
    BOOST_CHECK(CSubNet("1.2.3.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.2.0/24").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(CSubNet("1.2.3.4/32").Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("1.2.3.4").Match(CNetAddr("5.6.7.8")));
    BOOST_CHECK(!CSubNet("1.2.3.4/32").Match(CNetAddr("5.6.7.8")));
    BOOST_CHECK(CSubNet("::ffff:127.0.0.1").Match(CNetAddr("127.0.0.1")));
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:8")));
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8").Match(CNetAddr("1:2:3:4:5:6:7:9")));
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:0/112").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(CSubNet("192.168.0.1/24").Match(CNetAddr("192.168.0.2")));
    BOOST_CHECK(CSubNet("192.168.0.20/29").Match(CNetAddr("192.168.0.18")));
    BOOST_CHECK(CSubNet("1.2.2.1/24").Match(CNetAddr("1.2.2.4")));
    BOOST_CHECK(CSubNet("1.2.2.110/31").Match(CNetAddr("1.2.2.111")));
    BOOST_CHECK(CSubNet("1.2.2.20/26").Match(CNetAddr("1.2.2.63")));
    // All-Matching IPv6 Matches arbitrary IPv4 and IPv6
    BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    BOOST_CHECK(CSubNet("::/0").Match(CNetAddr("1.2.3.4")));
    // All-Matching IPv4 does not Match IPv6
    BOOST_CHECK(!CSubNet("0.0.0.0/0").Match(CNetAddr("1:2:3:4:5:6:7:1234")));
    // Invalid subnets Match nothing (not even invalid addresses)
    BOOST_CHECK(!CSubNet().Match(CNetAddr("1.2.3.4")));
    BOOST_CHECK(!CSubNet("").Match(CNetAddr("4.5.6.7")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("0.0.0.0")));
    BOOST_CHECK(!CSubNet("bloop").Match(CNetAddr("hab")));
    // Check valid/invalid
    BOOST_CHECK(CSubNet("1.2.3.0/0").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/-1").IsValid());
    BOOST_CHECK(CSubNet("1.2.3.0/32").IsValid());
    BOOST_CHECK(!CSubNet("1.2.3.0/33").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/0").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/33").IsValid());
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/-1").IsValid());
    BOOST_CHECK(CSubNet("1:2:3:4:5:6:7:8/128").IsValid());
    BOOST_CHECK(!CSubNet("1:2:3:4:5:6:7:8/129").IsValid());
    BOOST_CHECK(!CSubNet("fuzzy").IsValid());
}

BOOST_AUTO_TEST_CASE(netbase_getgroup)
{

    BOOST_CHECK(ResolveIP("127.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // Local -> !Routable()
    BOOST_CHECK(ResolveIP("257.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // !Valid -> !Routable()
    BOOST_CHECK(ResolveIP("10.0.0.1").GetGroup() == std::vector<unsigned char>({0})); // RFC1918 -> !Routable()
    BOOST_CHECK(ResolveIP("169.254.1.1").GetGroup() == std::vector<unsigned char>({0})); // RFC3927 -> !Routable()
    BOOST_CHECK(ResolveIP("1.2.3.4").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // IPv4
    BOOST_CHECK(ResolveIP("::FFFF:0:102:304").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6145
    BOOST_CHECK(ResolveIP("64:FF9B::102:304").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6052
    BOOST_CHECK(ResolveIP("2002:102:304:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC3964
    BOOST_CHECK(ResolveIP("2001:0:9999:9999:9999:9999:FEFD:FCFB").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC4380
    BOOST_CHECK(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_TOR, 239})); // Tor
    BOOST_CHECK(ResolveIP("2001:470:abcd:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 4, 112, 175})); //he.net
    BOOST_CHECK(ResolveIP("2001:2001:9999:9999:9999:9999:9999:9999").GetGroup() == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 32, 1})); //IPv6

    // baz.net sha256 hash: 12929400eb4607c4ac075f087167e75286b179c693eb059a01774b864e8fe505
    std::vector<unsigned char> internal_group = {NET_INTERNAL, 0x12, 0x92, 0x94, 0x00, 0xeb, 0x46, 0x07, 0xc4, 0xac, 0x07};
    BOOST_CHECK(CreateInternal("baz.net").GetGroup() == internal_group);
}

BOOST_AUTO_TEST_SUITE_END()
