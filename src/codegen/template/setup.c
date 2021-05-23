int lua_error_handler(lua_State *L) {
  char const *msg = lua_tostring(L, 1);

  if (msg == NULL) {
    if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING) {
      return 1;
    } else {
      char const *type = luaL_typename(L, 1);
      msg = lua_pushfstring(L, "(error object is a %s value)", type);
    }
  }

  luaL_traceback(L, L, msg, 1);
  return 1;
}

int lua_main(lua_State *L) {
  int status = luaL_loadbuffer(L, BT_GLUE, `LENGTH`, BT_GLUE);

  if (status != LUA_OK) {
    lua_error(L);
    return 0;
  }

  luaA_wrap_closure(L, L->top - 1, lua_func_0);

  int num_arg = lua_tointeger(L, 1);
  char **list_arg = lua_touserdata(L, 2);

  for (int i = 1; i < num_arg; i += 1) {
    lua_pushstring(L, list_arg[i]);
  }

  lua_call(L, num_arg - 1, 0);

  return 0;
}

int main(int argc, char *argv[]) {
  lua_State *L = luaL_newstate();

  if (L == NULL) {
    return 1;
  }

  luaL_openlibs(L);
  lua_gc(L, LUA_GCGEN, 0, 0);

  lua_pushcfunction(L, &lua_error_handler);
  lua_pushcfunction(L, &lua_main);
  lua_pushinteger(L, argc);
  lua_pushlightuserdata(L, argv);

  int status = lua_pcall(L, 2, 0, -4);

  if (status != LUA_OK) {
    char const *msg = lua_tostring(L, -1);

    lua_writestringerror("%s\n", msg);
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
  lua_close(L);

  return status == LUA_OK ? 0 : 1;
}
