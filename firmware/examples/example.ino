/*
 * Example using the LDAPClient library.
 */

#include "LDAPClient/LDAPClient.h"

void setup
{
	LDAPClient client;
	client.connect(IPAddress({10,0,1,6},1389);
	if (client.connected()) {
		client.bind("cn=Directory Manager", "password");
		client.read("dc=example,dc=com");
	}
}

void loop
{
}