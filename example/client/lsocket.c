/* lua-socket.c
 * author: xjdrew
 * date: 2014-07-10
 */

/*
This module provides an interface to Berkeley socket IPC.

Limitations:

- Only AF_INET, AF_INET6 address families are supported
- Only SOCK_STREAM, SOCK_DGRAM socket type are supported
- Only IPPROTO_TCP, IPPROTO_UDP protocal type are supported
- Don't support dns lookup, must be numerical network address

Module interface:
- socket.socket(family, type[, proto]) --> new socket object
- socket.AF_INET, socket.SOCK_STREAM, etc.: constants from <socket.h>
- socket.resolve(hostname), hostname can be anything recognized by getaddrinfo
*/

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include "lua.h"
#include "lauxlib.h"

#define SOCKET_METATABLE "socket_metatable"

#define RECV_BUFSIZE (4079)
/*
#if !defined(NI_MAXHOST)
#define NI_MAXHOST 1025
#endif

#if !defined(NI_MAXSERV)
#define NI_MAXSERV 32
#endif
*/

typedef struct _sock_t {
    int fd;
    int family;
    int type;
    int protocol;
} socket_t;

/* 
 *   internal function 
 */
inline static socket_t* 
_getsock(lua_State *L, int index) {
    socket_t* sock = (socket_t*)luaL_checkudata(L, index, SOCKET_METATABLE);
    return sock;
}

inline static void
_setsock(lua_State *L, int fd, int family, int type, int protocol) {
#ifdef SO_NOSIGPIPE
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&on, sizeof(on));
#endif

    socket_t *nsock = (socket_t*) lua_newuserdata(L, sizeof(socket_t));
    luaL_getmetatable(L, SOCKET_METATABLE);
    lua_setmetatable(L, -2);

    nsock->fd = fd;
    nsock->family = family;
    nsock->type = type;
    nsock->protocol = protocol;
}

static const char*
_addr2string(struct sockaddr *sa, char *buf, int buflen)
{
    const char *s;
    if (sa->sa_family == AF_INET)
        s = inet_ntop(sa->sa_family, (const void*) &((struct sockaddr_in*)sa)->sin_addr, buf, buflen);
    else
        s = inet_ntop(sa->sa_family, (const void*) &((struct sockaddr_in6*)sa)->sin6_addr, buf, buflen);
    return s;
}

static int
_getsockaddrarg(socket_t *sock, const char *host, const char *port, struct addrinfo **res) {
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = sock->family;
    hints.ai_socktype = sock->type;
    hints.ai_protocol = sock->protocol;
    hints.ai_flags = AI_NUMERICHOST;

    return getaddrinfo(host, port, &hints, res);
}

static int
_getsockaddrlen(socket_t *sock, socklen_t *len) {
    switch(sock->family) {
        case AF_INET:
            *len = sizeof(struct sockaddr_in);
            return 1;
        case AF_INET6:
            *len = sizeof(struct sockaddr_in6);
            return 1;
    }
    return 0;
}

static int
_makeaddr(lua_State *L, struct sockaddr *addr, int addrlen) {
    char ip[NI_MAXHOST];
    char port[NI_MAXSERV];
    int err = getnameinfo(addr, addrlen, ip, sizeof(ip), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
    if(err != 0) {
        lua_pushnil(L);
        lua_pushinteger(L, err);
        return 2;
    }
    lua_pushstring(L, ip);
    lua_pushstring(L, port);
    lua_tonumber(L, -1);
    return 2;
}

/*
 *    end
 */

/*
 *   args: int domain, int type, int protocal
 */
static int
_socket(lua_State *L) {
    int family    = luaL_checkinteger(L, 1);
    int type      = luaL_checkinteger(L, 2);
    int protocol  = 0;
    if(lua_gettop(L) > 2) {
        protocol = luaL_checkinteger(L, 3);
    }

    int fd = socket(family, type, protocol);
    if(fd < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    _setsock(L, fd, family, type, protocol);
    return 1;
}

static int
_resolve(lua_State *L) {
    const char* host = luaL_checkstring(L, 1);
    struct addrinfo *res = 0;

    int err = getaddrinfo(host, NULL, NULL, &res);
    if(err != 0) {
        lua_pushnil(L);
        lua_pushinteger(L, err);
        return 2;
    }

    int i = 1;
    char buf[64];
    lua_newtable(L);
    while(res) {
        // ignore all unsupported address
        if(res->ai_family == AF_INET || res->ai_family == AF_INET6) {
            lua_createtable(L, 0, 2);
            lua_pushinteger(L, res->ai_family);
            lua_setfield(L, -2, "family");
            lua_pushstring(L, _addr2string(res->ai_addr, buf, sizeof(buf)));
            lua_setfield(L, -2, "addr");
            lua_rawseti(L, -2, i++);
        }
        res = res->ai_next;
    }
    return 1;
}

static int
_strerror(lua_State *L) {
    int err = luaL_checkinteger(L, 1);
    lua_pushstring(L, strerror(err));
    return 1;
}

static int
_gai_strerror(lua_State *L) {
    int err = luaL_checkinteger(L, 1);
    lua_pushstring(L, gai_strerror(err));
    return 1;
}

/* socket object methods */
static int
_sock_setblocking(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int block = lua_toboolean(L, 2);
    int flag = fcntl(sock->fd, F_GETFL, 0);
    if (block) {
        flag &= (~O_NONBLOCK);
    } else {
        flag |= O_NONBLOCK;
    }
    fcntl(sock->fd, F_SETFL, flag);
    return 0;
}

static int
_sock_connect(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkinteger(L, 3);
    const char *port = lua_tostring(L, 3);

    struct addrinfo *res = 0;
    int err = _getsockaddrarg(sock, host, port, &res);
    if(err != 0) {
        lua_pushinteger(L, err);
        return 1;
    }

    err = connect(sock->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    
    int flag        =   1;
    setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag));

    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_check_async_connect(lua_State *L) {
    socket_t *sock = _getsock(L, 1);

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock->fd, &fdset);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int n = select(sock->fd+1, NULL, &fdset, NULL, &tv);

    // not ready
    if(n == 0) {
      lua_pushboolean(L, 0);
      return 1;
    }

    // error
    if(n < 0) {
      lua_pushboolean(L, 0);
      lua_pushinteger(L, errno);
      return 2;
    }

    int err;
    socklen_t errl = sizeof(err);
    if(getsockopt(sock->fd, SOL_SOCKET, SO_ERROR, &err, &errl) < 0) {
      lua_pushboolean(L, 0);
      lua_pushinteger(L, errno);
      return 2;
    }

    if(err) {
      lua_pushboolean(L, 0);
      lua_pushinteger(L, err);
      return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int
_sock_recv(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    char buf[RECV_BUFSIZE];
    ssize_t nread = recv(sock->fd, buf, RECV_BUFSIZE, 0);
    if(nread < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushlstring(L, buf, nread);
    return 1;
}

static int
_sock_send(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    size_t len;
    const char* buf = luaL_checklstring(L, 2, &len);
    size_t from = luaL_optinteger(L, 3, 0);
    int flags = 0;

#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif

    if (len <= from) {
        return luaL_argerror(L, 3, "should be less than length of argument #2");
    }

    int nwrite = send(sock->fd, buf+from, len - from, flags);
    if(nwrite < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushinteger(L, nwrite);
    return 1;
}

static int
_sock_recvfrom(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    size_t len = luaL_checkinteger(L, 2);
    socklen_t addr_len;
    if(!_getsockaddrlen(sock, &addr_len)) {
        return luaL_error(L, "getpeername: bad family(%d)", sock->family);
    }

    struct sockaddr *addr = (struct sockaddr*)lua_newuserdata(L, addr_len);
    char* buf = lua_newuserdata(L, len);

    int nread = recvfrom(sock->fd, buf, len, 0, addr, &addr_len);
    if(nread < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushlstring(L, buf, nread);
    return _makeaddr(L, addr, addr_len) + 1;
}

static int
_sock_sendto(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkinteger(L, 3);
    const char *port = lua_tostring(L, 3);
    size_t len;
    const char* buf = luaL_checklstring(L, 4, &len);
    int flags = 0;

#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif

    struct addrinfo *res = 0;
    int err = _getsockaddrarg(sock, host, port, &res);
    if(err != 0) {
        lua_pushnil(L);
        lua_pushinteger(L, err);
        return 1;
    }

    int nwrite = sendto(sock->fd, buf, len, flags, res->ai_addr, res->ai_addrlen);
    if(nwrite < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushinteger(L, nwrite);
    return 1;
}

static int
_sock_bind(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    const char *host = luaL_checkstring(L, 2);
    luaL_checkinteger(L, 3);
    const char *port = lua_tostring(L, 3);

    struct addrinfo *res = 0;
    int err = _getsockaddrarg(sock, host, port, &res);
    if(err != 0) {
        lua_pushinteger(L, err);
        return 1;
    }

    err = bind(sock->fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_listen(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int backlog = luaL_optnumber(L, 2, 5);
    int err = listen(sock->fd, backlog);
    if(err != 0) {
        lua_pushinteger(L, errno);
    } else {
        lua_pushinteger(L, err);
    }
    return 1;
}

static int
_sock_accept(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int fd = accept(sock->fd, NULL, NULL);
    if(fd < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    _setsock(L, fd, sock->family, sock->type, sock->protocol);
    return 1;
}

static int
_sock_fileno(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    lua_pushinteger(L, sock->fd);
    return 1;
}

static int
_sock_getpeername(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    socklen_t len;
    if(!_getsockaddrlen(sock, &len)) {
        return luaL_error(L, "getpeername: bad family(%d)", sock->family);
    }

    struct sockaddr *addr = (struct sockaddr*)lua_newuserdata(L, len);
    int err = getpeername(sock->fd, addr, &len);
    if(err < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    return _makeaddr(L, addr, len);
}

static int
_sock_getsockname(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    socklen_t len;
    if(!_getsockaddrlen(sock, &len)) {
        return luaL_error(L, "getsockname: bad family(%d)", sock->family);
    }

    struct sockaddr *addr = (struct sockaddr*)lua_newuserdata(L, len);
    int err = getsockname(sock->fd, addr, &len);
    if(err < 0) {
        lua_pushnil(L);
        lua_pushinteger(L, errno);
        return 2;
    }
    return _makeaddr(L, addr, len);
}

static int
_sock_getsockopt(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int level = luaL_checkinteger(L, 2);
    int optname = luaL_checkinteger(L, 3);
    socklen_t buflen = luaL_optinteger(L, 4, 0);

    if(buflen > 1024) {
        return luaL_error(L, "getsockopt buflen out of range");
    }

    int err;
    if(buflen == 0) {
        int flag = 0;
        socklen_t flagsize = sizeof(flag);
        err = getsockopt(sock->fd, level, optname, (void*)&flag, &flagsize);
        if(err < 0) {
            goto failed;
        }
        lua_pushinteger(L, flag);
    } else {
        void *optval = lua_newuserdata(L, buflen);
        err = getsockopt(sock->fd, level, optname, optval, &buflen);
        if(err < 0) {
            goto failed;
        }
        lua_pushlstring(L, optval, buflen);
    }
    return 1;
failed:
    lua_pushnil(L);
    lua_pushinteger(L, errno);
    return 2;
}

static int
_sock_setsockopt(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int level = luaL_checkinteger(L, 2);
    int optname = luaL_checkinteger(L, 3);
    luaL_checkany(L, 4);

    const char* buf;
    size_t buflen;

    int type = lua_type(L, 4);
    if(type == LUA_TSTRING) {
        buf = luaL_checklstring(L, 4, &buflen);
    } else if(type == LUA_TNUMBER) {
        int flag = luaL_checkinteger(L, 4);
        buf = (const char*)&flag;
        buflen = sizeof(flag);
    } else {
        return luaL_error(L, "setsockopt: unsupported arg 4 type");
    }

    int err = setsockopt(sock->fd, level, optname, buf, buflen);
    if(err < 0) {
        lua_pushboolean(L, 0);
        lua_pushinteger(L, errno);
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int
_sock_close(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    int fd = sock->fd;
    if(fd != -1) {
        sock->fd = -1;
        close(fd);
    }
    return 0;
}

static int
_sock_tostring(lua_State *L) {
    socket_t *sock = _getsock(L, 1);
    lua_pushfstring(L, "socket: %p", sock);
    return 1;
}

/* end */
static void
_add_uint_constant(lua_State *L, const char* name, unsigned int value) {
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
}

int
luaopen_lsocket(lua_State *L) {
    luaL_checkversion(L);
        
    // +construct socket metatable
    luaL_Reg socket_mt[] = {
        {"__gc", _sock_close},
        {"__tostring",  _sock_tostring},
        {NULL, NULL}
    };

    luaL_Reg socket_methods[] = {
        {"setblocking", _sock_setblocking},
        {"connect", _sock_connect},

        {"check_async_connect", _sock_check_async_connect},

        {"recv", _sock_recv},
        {"send", _sock_send},

        {"recvfrom", _sock_recvfrom},
        {"sendto", _sock_sendto},

        {"bind", _sock_bind},
        {"listen", _sock_listen},
        {"accept", _sock_accept},

        {"fileno", _sock_fileno},
        {"getpeername", _sock_getpeername},
        {"getsockname", _sock_getsockname},

        {"getsockopt", _sock_getsockopt},
        {"setsockopt", _sock_setsockopt},

        {"close", _sock_close},
        {NULL, NULL}
    };

    luaL_newmetatable(L, SOCKET_METATABLE);
    luaL_setfuncs(L, socket_mt, 0);

    luaL_newlib(L, socket_methods);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
    // +end

    luaL_Reg l[] = {
        {"socket", _socket},
        {"resolve", _resolve},
        {"strerror", _strerror},
        {"gai_strerror", _gai_strerror},
        {NULL, NULL}
    };

    luaL_newlib(L, l);
    // address family
    _add_uint_constant(L, "AF_INET", AF_INET);
    _add_uint_constant(L, "AF_INET6", AF_INET6);

    // socket type
    _add_uint_constant(L, "SOCK_STREAM", SOCK_STREAM);
    _add_uint_constant(L, "SOCK_DGRAM", SOCK_DGRAM);

    // protocal type
    _add_uint_constant(L, "IPPROTO_TCP", IPPROTO_TCP);
    _add_uint_constant(L, "IPPROTO_UDP", IPPROTO_UDP);

    // sock opt
    _add_uint_constant(L, "SOL_SOCKET", SOL_SOCKET);

    _add_uint_constant(L, "SO_REUSEADDR", SO_REUSEADDR);
    _add_uint_constant(L, "SO_LINGER", SO_LINGER);
    _add_uint_constant(L, "SO_KEEPALIVE", SO_KEEPALIVE);
    _add_uint_constant(L, "SO_SNDBUF", SO_SNDBUF);
    _add_uint_constant(L, "SO_RCVBUF", SO_RCVBUF);
#ifdef SO_REUSEPORT
    _add_uint_constant(L, "SO_REUSEPORT", SO_REUSEPORT);
#endif
#ifdef SO_NOSIGPIPE
    _add_uint_constant(L, "SO_NOSIGPIPE", SO_NOSIGPIPE);
#endif 
#ifdef SO_NREAD
    _add_uint_constant(L, "SO_NREAD", SO_NREAD);
#endif
#ifdef SO_NWRITE
    _add_uint_constant(L, "SO_NWRITE", SO_NWRITE);
#endif
#ifdef SO_LINGER_SEC
    _add_uint_constant(L, "SO_LINGER_SEC", SO_LINGER_SEC);
#endif

    // errno
    _add_uint_constant(L, "OK", 0);
    _add_uint_constant(L, "EINTR", EINTR);
    _add_uint_constant(L, "EAGAIN", EAGAIN);
    _add_uint_constant(L, "EINPROGRESS", EINPROGRESS);
    _add_uint_constant(L, "ECONNREFUSED", ECONNREFUSED);
    _add_uint_constant(L, "EISCONN", EISCONN);

    // getaddrinfo errors
    _add_uint_constant(L, "EAI_AGAIN", EAI_AGAIN);
    _add_uint_constant(L, "EAI_BADFLAGS", EAI_BADFLAGS);
    //_add_uint_constant(L, "EAI_BADHINTS", EAI_BADHINTS);
    _add_uint_constant(L, "EAI_FAIL", EAI_FAIL);
    _add_uint_constant(L, "EAI_FAMILY", EAI_FAMILY);
    _add_uint_constant(L, "EAI_MEMORY", EAI_MEMORY);
    _add_uint_constant(L, "EAI_NONAME", EAI_NONAME);
    _add_uint_constant(L, "EAI_OVERFLOW", EAI_OVERFLOW);
    //_add_uint_constant(L, "EAI_PROTOCOL", EAI_PROTOCOL);
    _add_uint_constant(L, "EAI_SERVICE", EAI_SERVICE);
    _add_uint_constant(L, "EAI_SOCKTYPE", EAI_SOCKTYPE);
    _add_uint_constant(L, "EAI_SYSTEM", EAI_SYSTEM);

    _add_uint_constant(L, "VERSION", 2);

    return 1;
}

