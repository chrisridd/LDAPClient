#include "LDAPClient.h"

/* Common tags */
// primitive elements
#define UNIV_BOOLEAN          0x01
#define UNIV_INTEGER          0x02
#define UNIV_OCTETSTRING      0x04
#define UNIV_ENUMERATED       0x0A
// other (primitive or constructed) elements
#define UNIV_SEQUENCE_CONS    0x30
#define UNIV_SET_CONS         0x31
#define APPLICATION_0         0x40
#define APPLICATION_0_CONS    0x60
#define APPLICATION_1         0x41
#define APPLICATION_1_CONS    0x61
#define APPLICATION_3_CONS    0x63
#define APPLICATION_4_CONS    0x64
#define APPLICATION_5_CONS    0x65
#define CONTEXT_0             0x80
#define CONTEXT_0_CONS        0xA0
#define CONTEXT_7             0x87

/* Encode a tag and a length */
// returns 0 (error)
// returns 2 (lengths < 128)
// returns 3 (lengths < 256)
// returns 4 (lengths < 65536)
static size_t encodeTagLength(byte *bytes, int tag, size_t len)
{
  int i = 0;
  if (tag > 255) {
    Serial.println("Tag too large");
    return 0;
  }
  bytes[i++] = (byte)tag;
  if (len <= 0x7f) {
    bytes[i++] = (byte)len;
  } else if (len <= 0xff) {
    bytes[i++] = 0x81;
    bytes[i++] = (byte)len;
  } else if (len <= 0xffff) {
    bytes[i++] = 0x82;
    bytes[i++] = (byte)(len >> 8);
    bytes[i++] = (byte)(len & 0xff);
  } else {
    Serial.println("Length too large");
    return 0;
  }
  return i;
}

/* Encode a complete INTEGER value */
// returns number of bytes written
static size_t encodeTagLengthINTEGER(byte *bytes, int tag, int value)
{
  size_t len = 0;
  if (value >= -128 && value <= 127) {
    len = encodeTagLength(bytes, tag, 1);
    bytes[len++] = (byte)(value >> 0) & 0xff;
  } else if (value >= -32768 && value <= 32767) {
    len = encodeTagLength(bytes, tag, 2);
    bytes[len++] = (byte)(value >> 8) & 0xff;
    bytes[len++] = (byte)(value >> 0) & 0xff;
  }
  return len;
}

/* Encode a complete BOOLEAN value */
// returns number of bytes written
static size_t encodeTagLengthBOOLEAN(byte *bytes, int tag, bool value)
{
  encodeTagLength(bytes, tag, 1);
  bytes[2] = value ? 0xff : 0x00;
  return 3;
}

// return false if an error
bool LDAPClient::readTagLength(int *tag, int *length)
{
  byte buffer[4];
  size_t len = 0;
  while (millis() < _pdutime) {
    Spark.process();
    if (_client.available() > 0) {
      ++_pdupos;
      buffer[len++] = _client.read();

      if (len == 2 && buffer[1] <= 0x7f) {
        *tag = (int)buffer[0];
        *length = (int)buffer[1];
        return true;
      }
      if (len == 3 && buffer[1] == 0x81) {
        *tag = (int)buffer[0];
        *length = (int)buffer[2];
        return true;
      }
      if (len == 4 && buffer[1] == 0x82) {
        *tag = (int)buffer[0];
        *length = (((int)buffer[2] & 0xff) << 8) |
          ((int)buffer[3] & 0xff);
        return true;
      }
    }
  }
  Serial.println("readTagLength failed");
  return false;
}

bool LDAPClient::decodeINTEGER(int tag, int *value)
{
  int the_tag, the_length;
  if (!readTagLength(&the_tag, &the_length)) {
    Serial.println("decodeINTEGER failed");
    return false;
  }
  if (the_tag != tag) {
    Serial.println("decodeINTEGER wrong tag");
    return false;
  }

  byte buffer[2];
  size_t len = 0;
  while (millis() < _pdutime) {
    if (_client.available() > 0) {
      ++_pdupos;
      buffer[len++] = _client.read();
      if (len == the_length) {
        if (len == 1) {
          *value = (int)(buffer[0]);
          return true;
        }
        if (len == 2) {
          *value = ((int)buffer[0] << 8) |
            ((int)buffer[1] << 0);
            return true;
        }
        return false;
      }
    }
  }
  return false;
}

bool LDAPClient::decodeSTRING(int tag, String &value)
{
  int the_tag, the_length;
  if (!readTagLength(&the_tag, &the_length)) {
    Serial.println("decodeSTRING failed");
    return false;
  }
  if (the_tag != tag) {
    Serial.println("decodeSTRING wrong tag");
    return false;
  }

  value.reserve(the_length);

  if (the_length == 0) return true;

  size_t len = 0;
  while (millis() < _pdutime) {
    if (_client.available() > 0) {
      ++_pdupos;
      ++len;
      value += (char)_client.read();
      if (len == the_length) {
        return true;
      }
    }
  }
  return false;
}

LDAPClient::~LDAPClient()
{
  if (_connected) {
    _client.stop();
  }
}

void LDAPClient::connect(IPAddress &host, int port)
{
  _client.connect(host, port);
  _connected = true;
}

/*
  LDAPMessage ::= SEQUENCE {
      messageID       INTEGER,
      bindRequest [APPLICATION 0] SEQUENCE {
        version                 INTEGER (1 ..  127),
        name                    LDAPDN,
        authentication          [0] OCTET STRING
      },
      controls       [0] Controls OPTIONAL }

  LDAPMessage ::= SEQUENCE {
      messageID       INTEGER,
      bindResponse [APPLICATION 1] SEQUENCE {
        resultCode        ENUMERATED,
        matchedDN         LDAPDN,
        diagnosticMessage LDAPDN,
        referral          [3] Referral OPTIONAL
      },
      controls       [0] Controls OPTIONAL }

*/
void LDAPClient::bind(const char dn[], const char password[])
{
  Serial.println("Send bind request");
  _msgid++;
  // construct the message from the inside out to avoid
  // memory allocations.
  byte version[3]; // INTEGER 3
  size_t version_len = encodeTagLengthINTEGER(version, UNIV_INTEGER, 3);

  byte name[4]; // tag + len
  size_t name_len = encodeTagLength(name, UNIV_OCTETSTRING, strlen(dn));

  byte authentication[4]; // tag + len
  size_t authentication_len = encodeTagLength(authentication, CONTEXT_0, strlen(password));

  byte bindrequest[4]; // APPLICATION 0 + len
  size_t bindrequest_content_len = version_len + name_len + strlen(dn) + authentication_len + strlen(password);
  size_t bindrequest_len = encodeTagLength(bindrequest, APPLICATION_0_CONS, bindrequest_content_len);

  byte messageid[6]; // tag + len(max 4) + int
  size_t messageid_len = encodeTagLengthINTEGER(messageid, UNIV_INTEGER, _msgid);
  size_t message_content_len = messageid_len + bindrequest_len + bindrequest_content_len;

  byte ldapmessage[4]; // SEQUENCE + len
  size_t ldapmessage_len = encodeTagLength(ldapmessage, UNIV_SEQUENCE_CONS, message_content_len);

  // Now write the bytes out in the actual order
  _client.write(ldapmessage, ldapmessage_len);
  _client.write(messageid, messageid_len); // includes value
  _client.write(bindrequest, bindrequest_len);
  _client.write(version, version_len); // includes value
  _client.write(name, name_len);
  _client.write((byte *)dn, strlen(dn));
  _client.write(authentication, authentication_len);
  _client.write((byte *)password, strlen(password));

  Serial.println("Wait for bind response");

  unsigned long _pdutime = millis() + _timeout;
  _pdupos = 0;
  _error = -1;

  int pdu_tag, pdu_length;
  if (readTagLength(&pdu_tag, &pdu_length)) {
    if (pdu_tag != UNIV_SEQUENCE_CONS) {
      Serial.println("Bad opening tag");
      return;
    }
    Serial.println("Got PDU start");
    int messageid_value;
    if (!decodeINTEGER(UNIV_INTEGER, &messageid_value) || messageid_value != _msgid) {
      Serial.println("Error at messageid");
      return;
    }
    Serial.println("Got messageid");
    int msg_tag = 0, msg_len;
    if (!readTagLength(&msg_tag, &msg_len) || msg_tag != APPLICATION_1_CONS) {
      Serial.print("Error at bindResponse ");
      Serial.println(msg_tag);
      return;
    }
    Serial.println("Got bindResponse");

    int resultCode;
    String matchedDN((char *)NULL);
    String diagnosticMessage((char *)NULL);
    if (!decodeResult(&resultCode, matchedDN, diagnosticMessage)) {
      Serial.println("Error at resultCode");
      return;
    }
    Serial.println("Result code: " + String(resultCode));
    Serial.println("Matched DN: " + matchedDN);
    Serial.println("Diagnostic Message: " + diagnosticMessage);

    _error = resultCode;
    _client.flush();
    return;
  }
  _error = 0;
}

/*
 * Decode a generic result
 */
bool LDAPClient::decodeResult(int *resultCode, String &matchedDN, String &diagnosticMessage)
{
  int resultcode_value;
  if (!decodeINTEGER(UNIV_ENUMERATED, &resultcode_value)) {
    Serial.println("Error at resultCode");
    return false;
  }

  if (!decodeSTRING(UNIV_OCTETSTRING, matchedDN)) {
    Serial.println("Error at matchedDN");
    return false;
  }

  if (!decodeSTRING(UNIV_OCTETSTRING, diagnosticMessage)) {
    Serial.println("Error at diagnosticMessage");
    return false;
  }
  return true;
}

/*
  LDAPMessage ::= SEQUENCE {
      messageID       INTEGER,
      searchRequest [APPLICATION 3] SEQUENCE {
          baseObject      LDAPDN,
          scope           ENUMERATED,
          derefAliases    ENUMERATED,
          sizeLimit       INTEGER (0 ..  maxInt),
          timeLimit       INTEGER (0 ..  maxInt),
          typesOnly       BOOLEAN,
          present         [7] LDAPString,
          attributes      SEQUENCE OF LDAPString },
      controls       [0] Controls OPTIONAL }

  LDAPMessage ::= SEQUENCE {
      messageID       INTEGER,
      bindResponse [APPLICATION 1] SEQUENCE {
        resultCode        ENUMERATED,
        matchedDN         LDAPDN,
        diagnosticMessage LDAPDN,
        referral          [3] Referral OPTIONAL
      },
      controls       [0] Controls OPTIONAL }

*/
void LDAPClient::read(const char dn[])
{
  _msgid++;
  // construct the message from the inside out to avoid
  // memory allocations.
  byte base[4];
  size_t base_len = encodeTagLength(base, UNIV_OCTETSTRING, strlen(dn));
  byte scope[3];
  encodeTagLengthINTEGER(scope, UNIV_ENUMERATED, 0);
  byte deref[3];
  encodeTagLengthINTEGER(deref, UNIV_ENUMERATED, 3);
  byte sizelimit[3];
  encodeTagLengthINTEGER(sizelimit, UNIV_INTEGER, 0);
  byte timelimit[3];
  encodeTagLengthINTEGER(timelimit, UNIV_INTEGER, 0);
  byte typesonly[3];
  encodeTagLengthBOOLEAN(typesonly, UNIV_BOOLEAN, false);
  byte filter[3];
  size_t filter_len = encodeTagLength(filter, CONTEXT_7, strlen("objectClass"));
  byte attributes[2];
  encodeTagLength(attributes, UNIV_SEQUENCE_CONS, 0);

  size_t searchrequest_content_len = base_len + strlen(dn) +
    sizeof(scope) +
    sizeof(deref) +
    sizeof(sizelimit) +
    sizeof(timelimit) +
    sizeof(typesonly) +
    filter_len + strlen("objectClass") +
    sizeof(attributes);

  byte searchrequest[4];
  size_t searchrequest_len = encodeTagLength(searchrequest, APPLICATION_3_CONS, searchrequest_content_len);

  byte messageid[6]; // tag + len(max 4) + int
  size_t messageid_len = encodeTagLengthINTEGER(messageid, UNIV_INTEGER, _msgid);
  size_t message_content_len = messageid_len + searchrequest_len + searchrequest_content_len;

  byte ldapmessage[4]; // SEQUENCE + len
  size_t ldapmessage_len = encodeTagLength(ldapmessage, UNIV_SEQUENCE_CONS, message_content_len);

  _client.write(ldapmessage, ldapmessage_len);
  _client.write(messageid, messageid_len); // includes value
  _client.write(searchrequest, searchrequest_len);
  _client.write(base, base_len);
  _client.write((byte *)dn, strlen(dn));
  _client.write(scope, sizeof(scope));
  _client.write(deref, sizeof(deref));
  _client.write(sizelimit, sizeof(sizelimit));
  _client.write(timelimit, sizeof(timelimit));
  _client.write(typesonly, sizeof(typesonly));
  _client.write(filter, filter_len);
  _client.write((byte *)"objectClass", strlen("objectClass"));
  _client.write(attributes, sizeof(attributes));

  Serial.println("Wait for search responses");

  unsigned long _pdutime = millis() + _timeout;
  _pdupos = 0;
  _error = -1;

  bool done = false;

  while (!done) {
    int pdu_tag, pdu_length;
    if (readTagLength(&pdu_tag, &pdu_length)) {
      if (pdu_tag != UNIV_SEQUENCE_CONS) {
        Serial.println("Bad opening tag");
        return;
      }
      int messageid_value;
      if (!decodeINTEGER(UNIV_INTEGER, &messageid_value) || messageid_value != _msgid) {
        Serial.println("Error at messageid");
        return;
      }
      int msg_tag = 0, msg_len;
      if (!readTagLength(&msg_tag, &msg_len)) {
        Serial.println("Error at protocolOp");
        return;
      }
      // Search Result Entry
      if (msg_tag == APPLICATION_4_CONS) {
        String dn((char *)NULL);
        if (!decodeSTRING(UNIV_OCTETSTRING, dn)) {
          Serial.println("DN read");
          return;
        }
        Serial.println("dn: " + dn);

        int attrs_tag, attrs_len;
        if (!readTagLength(&attrs_tag, &attrs_len) || attrs_tag != UNIV_SEQUENCE_CONS) {
          Serial.println("Error at attributes sequence");
          return;
        }
        int endpos = _pdupos + attrs_len;
        while (_pdupos < endpos) {
          int attr_tag, attr_len;
          if (!readTagLength(&attr_tag, &attr_len) || attr_tag != UNIV_SEQUENCE_CONS) {
            Serial.println("Error at attribute sequence");
            return;
          }
          String attribute((char *)NULL);
          if (!decodeSTRING(UNIV_OCTETSTRING, attribute)) {
            Serial.println("Attribute read");
            return;
          }

          int set_tag, set_len;
          if (!readTagLength(&set_tag, &set_len) || set_tag != UNIV_SET_CONS) {
            Serial.println("Error at value set");
            return;
          }
          int endset = _pdupos + set_len;
          while (_pdupos < endset) {
            String value((char *)NULL);
            if (!decodeSTRING(UNIV_OCTETSTRING, value)) {
              Serial.println("Value read");
              return;
            }
            Serial.println(attribute + ": " + value);
          }
        }
        Serial.println();
        return;
      }
      // Search Result Done
      if (msg_tag == APPLICATION_5_CONS) {
        int resultCode;
        String matchedDN((char *)NULL);
        String diagnosticMessage((char *)NULL);
        if (!decodeResult(&resultCode, matchedDN, diagnosticMessage)) {
          Serial.println("Error at resultCode");
          return;
        }
        Serial.println("Result code: " + String(resultCode));
        Serial.println("Matched DN: " + matchedDN);
        Serial.println("Diagnostic Message: " + diagnosticMessage);

        _error = resultCode;
        done = true;
      }
    }
  }
}

int LDAPClient::error()
{
  return _error;
}
