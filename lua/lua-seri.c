#include "seri.h"

#include <lauxlib.h>

#include <stdlib.h>

static int
lpackstring(lua_State *L) {
	luaseri_pack(L);
	char * str = (char *)lua_touserdata(L, -2);
	int sz = lua_tointeger(L, -1);
	lua_pushlstring(L, str, sz);
	free(str);
	return 1;
}

int
luaopen_lseri(lua_State* L) {
	luaL_checkversion(L);

	luaL_Reg l[] = {
		{"pack", luaseri_pack},
		{"unpack", luaseri_unpack},
		{"packstring", lpackstring},
		{NULL, NULL}
	};

	luaL_newlib(L, l);
	return 1;
}