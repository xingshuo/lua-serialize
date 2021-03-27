--timestamp数值字串反序,取高5位
math.randomseed(tostring(os.time()):reverse():sub(1, 5))

local M = {}

function M.table_str(mt, max_floor, cur_floor)
    cur_floor = cur_floor or 1
    max_floor = max_floor or 5
    if max_floor and cur_floor > max_floor then
        return tostring(mt)
    end
    local str
    if cur_floor == 1 then
        str = string.format("%s{\n",string.rep("--",max_floor))
    else
        str = "{\n"
    end
    for k,v in pairs(mt) do
        if type(v) == 'table' then
            v = M.table_str(v, max_floor, cur_floor+1)
        else
            if type(v) == 'string' then
                v = "'" .. v .. "'"
            end
            v = tostring(v) .. "\n"
        end
        str = str .. string.format("%s[%s] = %s",string.rep("--",cur_floor),k,v)
    end
    str = str .. string.format("%s}\n",string.rep("--",cur_floor-1))
    return str
end

function M.is_same_table(t1, t2)
    -- check t1 <= t2
    for k1,v1 in pairs(t1) do
        local v2 = t2[k1]
        if type(v2) ~= type(v1) then
            return false
        end
        if type(v1) == 'table' then
            if not M.is_same_table(v1, v2) then
                return false
            end
        else
            if v1 ~= v2 then
                return false
            end
        end
    end
    -- check t2 <= t1
    for k2,v2 in pairs(t2) do
        local v1 = t1[k2]
        if type(v1) ~= type(v2) then
            return false
        end
        if type(v2) == 'table' then
            if not M.is_same_table(v2, v1) then
                return false
            end
        else
            if v2 ~= v1 then
                return false
            end
        end
    end

    return true
end

function M.random_str(n)
    local rand_str = ""
    local rand_num = 0
    for i=1,n do
        if math.random(1,3)==1 then
            rand_num = string.char(math.random(0,25)+65)   --生成大写字母 random(0,25)生成0=< <=25的整数
        elseif math.random(1,3)==2 then
            rand_num = string.char(math.random(0,25)+97)   --生成小写字母
        else
            rand_num = math.random(0,9)                   --生成0=< and <=9的随机数字
        end
        rand_str = rand_str .. rand_num
    end
    return rand_str
end

return M