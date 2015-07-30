#ifndef LDAPClient_h
#define LDAPClient_h

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#elif defined(SPARK)
#include "application.h"
#endif

#include <vector>
#include <map>

class LDAPClient
{
public:
  LDAPClient(int timeout = 1000) : _timeout(timeout), _connected(false), _error(0), _msgid(0) { };
  ~LDAPClient();
  bool connected() { return _client.connected(); };
  // Must be called first.
  void connect(IPAddress &host, int port = 389);
  // Authenticate.
  void bind(const char dn[], const char password[]);
  // Read a single entry.
  void read(const char dn[]);
  // LDAP error returned by last operation.
  int error();
private:
  bool readTagLength(int *tag, int *length);
  bool decodeINTEGER(int tag, int *value);
  bool decodeSTRING(int tag, String &value);
  bool decodeResult(int *resultCode, String &matchedDN, String &diagnosticMessage);

  int _timeout;
  int _pdutime;
  int _pdupos;
  bool _connected;
  TCPClient _client;
  int _error;
  int _msgid;
  std::map<String, std::vector<String> >_attrs;
};

#endif
