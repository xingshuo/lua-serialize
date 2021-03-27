-- 需要在当前文件夹下先执行make生成lseri.so
package.cpath = package.cpath .. ";lua/?.so"

local lseri = require "lseri"

function check_args(...)
    local args = {...}
    assert(args[1] == 500)
    assert(args[2] =="abcd")
    assert(args[3] == false)
    assert(args[4] == "lakefu")
    assert(args[5] == nil)
    assert(args[6][2] == nil)
    assert(args[6][3] == 8000)
    assert(args[6][4][7000] == "good")
    assert(args[6][9.9] == true)
end

local t1 = os.time()

local buffer, sz = lseri.pack(500, "abcd", false, "lakefu", nil,
        {
            "school",nil,8000, {key = 90, [7000] = "good"},
            ["tencent alibaba huawei bytedance pinduoduo"] = "996",
            [9.9] = true,
        }
)
check_args(lseri.unpack(buffer, sz))
print("--- PASS: lseri.pack")

local str = lseri.packstring(500, "abcd", false, "lakefu", nil,
        {
            "school",nil,8000, {key = 90, [7000] = "good"},
            ["tencent alibaba huawei bytedance pinduoduo"] = "996",
            [9.9] = true,
        }
)
check_args(lseri.unpack(str))
print("--- PASS: lseri.packstring")

local t2 = os.time()
print(string.format("ok %ss",t2 - t1))