LuaSerialize
====
    一种golang和lua无schema序列化方案的实现
类型对应
----
    golang中定义lua table结构:
    type Table struct {
        // 对应lua table 数组段
        Array []interface{}
        // 对应lua table 哈希段
        Hashmap map[interface{}]interface{}
    }
    ____________________________
    |  golang <---------> lua   |
    |___________________________|
    宏观
       interface{}   <---> TValue
    微观
       int64,float64 <---> number
       string        <---> string
       bool          <---> boolean
       nil           <---> nil
       Table(struct) <---> table
       
Table间映射关系
----
    以道具列表定义为例:
    'golang':
        items := &lua_seri.Table{
            Array: []interface{}{
                &lua_seri.Table{
                    Hashmap: map[interface{}]interface{}{
                        "ID":    int64(21001),
                        "Count": int64(100),
                    },
                },
                &lua_seri.Table{
                    Hashmap: map[interface{}]interface{}{
                        "ID":    int64(31001),
                        "Count": int64(200),
                    },
                },
                &lua_seri.Table{
                    Hashmap: map[interface{}]interface{}{
                        "ID":    int64(41001),
                        "Count": int64(300),
                    },
                },
            },
            Hashmap: map[interface{}]interface{}{
                "Diamond": int64(800),
                "Gold":    999.99,
            },
        }
    'lua':
        local items = {
            {ID = 21001, Count = 100},
            {ID = 31001, Count = 200},
            {ID = 41001, Count = 300},
            Diamond = 800,
            Gold = 999.99,
        }
    
实现原理
----
   序列化算法采用云风的skynet框架中snlua服务间rpc通信使用的序列化算法:
   https://github.com/cloudwu/lua-serialize
   
Tips
----
    1. 整个算法库分为基于原c库修改后c版本和独立实现的go版本2部分
    2. go版本的number类型只支持int64,float64两种, 移除了对应c版本的lightusrdata类型(单个go进程内部rpc一般不需要序列化, 而go和lua间rpc一般跨进程, 没必要传递指针)
    3. 整个算法库约定按[大端序]序列化和反序列化>=2字节的整形和浮点型(skynet使用的原版本c库没做统一约定, 不同端序的物理机间rpc会有问题)

单元测试
----
    go版本:  go test -v
    clua版本: lua lua/seri_test.lua (先执行make -C lua/ 编译lseri.so)

rpc测试示例
----
    cd example
    make
    ./server/main.exe     #run server
    lua client/main.lua   #run client

性能测试(go版本)
----
    go test -bench=.

性能报告(go版本)
----
    测试环境:
      AMD EPYC 7K62 CPU @ 2595MHz
      cpu逻辑核数: 1核
    测试数据:
      BenchmarkLuaSeri    	  126991	      9604 ns/op
      BenchmarkJson       	   40058	     29801 ns/op
      LuaSeri pkg len:260
      Json    pkg len:349
    测试结论:
      LuaSeri压解包性能约为Json 3倍
      LuaSeri压缩包体大小约为Json 75%