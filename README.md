About
===

This is a library which allows an application on the Particle (née Spark) Photon
to query an LDAP directory server.

## Usage

Create an `LDAPClient` object the same way you would create a `TCPClient` object:

```
LDAPClient client;
```

This doesn't do anything yet, so the next step is to connect to an LDAP server.
Currently only plaintext LDAP is supported due to a lack of SSL libraries on the
Photon:

```
client.connect(.....);
```

Note if you don't specify the final `port` parameter, it defaults to the LDAP
standard port of 389.

This will connect you to the LDAP server. You should now wait until you really
are connected, and only then continue with LDAP calls:

```
if (client.connected()) {
  // do more LDAP calls now
}
```

Initially you will have what LDAP calls "anonymous" access, that is, you haven't
authenticated to the server at all and you can only access some information in
the server. Maybe this is enough, but if not, you can authenticate using this
call:

```
client.bind("uid=xyz,ou=People,dc=example,dc=com", "password");
```

The first argument is the LDAP `DN` of the user you are authenticating as. It
looks like a string of `type=value` pairs separated by commas. See RFC 4512 and
RFC 4514 for the full details.

**Caution** the `password` is sent to the LDAP server in the clear, due to no
SSL. You should not use this over the wider Internet.

Finally, you can read information from the server. The first way simply "reads"
a given entry:

```
client.read("cn=Some Entry,ou=Somewhere Else,dc=example,dc=com");
```

## To Do

The error handling, and in fact all the result handling, is not really done
yet. I think it only makes sense to support one operation at a time, so my
thought is to have methods like `client.error()` to return the last operation's
result, etc.

Generic search can be easily implemented, as `client.read()` is already doing
all the necessary work.