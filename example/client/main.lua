package.cpath = package.cpath .. ";client/?.so"
package.path = package.path .. ";client/?.lua"

local lsocket = require "lsocket"
local lseri = require "lseri"
local utils = require "utils"

local IP, PORT = ...

if not IP then
    IP = "127.0.0.1"
end

if not PORT then
    PORT = 5051
else
    PORT = tonumber(PORT)
end

-- 包头: 4字节大端序
local HEAD_LEN = 4
local PACK_FMT  = string.format(">I%d",HEAD_LEN)

function create_channel(ip, port)
    -- 默认阻塞模式
    local fd, errno = lsocket.socket(lsocket.AF_INET, lsocket.SOCK_STREAM)
    if errno ~= nil then
        error(string.format("new socket err:%s", lsocket.strerror(errno)))
    end
    -- connect host
    local errno = fd:connect(ip, port)
    if errno ~= lsocket.OK then
        error(string.format("socket connect %s:%d err:%s", ip, port, lsocket.strerror(errno)))
    end
    return fd
end

function unary_call(fd, method, ...)
    -- pack
    local body = lseri.packstring(method, ...)
    local head = string.pack(PACK_FMT,#body)
    local pkg = head .. body
    -- send request
    local wn, errno = fd:send(pkg)
    if errno ~= nil then
        error(string.format("socket send err:%s", lsocket.strerror(errno)))
    end
    assert(wn == #pkg, "write len err, impossible")
    -- recv reply
    local recv_buffer = ""
    ::continue::
    local data, errno = fd:recv()
    if errno ~= nil then
        if errno == lsocket.EINTR then -- signal occurred ? retry
            goto continue
        end
        error(string.format("socket recv err:%s", lsocket.strerror(errno)))
    end
    if data == "" then
        error("socket peer closed")
    end
    recv_buffer = recv_buffer .. data
    if #recv_buffer < HEAD_LEN then
        goto continue
    end
    local body_len, next_pos = string.unpack(PACK_FMT, recv_buffer)
    if #recv_buffer < body_len + HEAD_LEN then
        goto continue
    end
    assert(#recv_buffer == body_len + HEAD_LEN, "reply stream overflow")
    local stream = recv_buffer:sub(next_pos)
    return lseri.unpack(stream)
end

function query_item(fd, token)
    -- 玩家ID
    local gid = 1001
    -- 是否男性
    local is_boy = true
    -- 玩家属性
    local property = {
        527, 1990, "SSD", 1119,
        level = 20,
        name = "lakefu",
        attrs = {
            [100] = "hp",
            mp = 80.75
        }
    }
    -- 一元式rpc调用
    local ok, rsp_items, null, rsp_token = unary_call(fd, "OnQueryItem", token, gid, is_boy, property)
    -- 检查回包参数
    assert(ok == true, "reply status err")
    local items = {
        {ID = 21001, Count = 100},
        {ID = 31001, Count = 200},
        {ID = 41001, Count = 300},
        Diamond = 800,
        Gold = 999.99,
    }

    assert(utils.is_same_table(rsp_items, items), "reply items err")
    assert(null == nil, "reply nil err")
    -- 完成token校验
    assert(rsp_token == token, "reply token err")
    print(string.format("recv items by token %s",token))
    print(utils.table_str(rsp_items))
end


local fd = create_channel(IP, PORT)
query_item(fd, utils.random_str(16))
query_item(fd, utils.random_str(32))
fd:close()