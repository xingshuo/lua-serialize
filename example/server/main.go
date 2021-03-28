package main

import (
	"bytes"
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"log"
	"net"

	lua_seri "github.com/xingshuo/lua-serialize"
)

const (
	HeadLen = 4
)

var (
	addr string
)

func Assert(condition bool, errMsg string) {
	if !condition {
		panic(errMsg)
	}
}

func OnQueryItem(args ...interface{}) []interface{} {
	token := args[0].(string)

	gid := args[1].(int64)
	Assert(gid == 1001, fmt.Sprintf("gid err %d != 1001", gid))

	isBoy := args[2].(bool)
	Assert(isBoy, "sex err")

	property := args[3].(*lua_seri.Table)
	// 校验数组段
	Assert(len(property.Array) == 4, "table array len err")
	Assert(property.Array[0].(int64) == 527 && property.Array[1].(int64) == 1990 &&
		property.Array[2].(string) == "SSD" && property.Array[3].(int64) == 1119,
		fmt.Sprintf("table array data err, %v", property.Array))
	// 校验哈希段
	Assert(property.Hashmap["level"] == int64(20), fmt.Sprintf("level err %v != 20", property.Hashmap["level"]))
	Assert(property.Hashmap["name"] == "lakefu", "name err")
	attrs := property.Hashmap["attrs"].(*lua_seri.Table)
	Assert(attrs.Hashmap[int64(100)].(string) == "hp", "hp err")
	Assert(attrs.Hashmap["mp"].(float64) == 80.75, "hp err")

	// 构造道具结构
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
	log.Printf("reply items by token %s\n%+v\n", token, items)
	return []interface{}{true, items, nil, token}
}

type rpcHandler func(...interface{}) []interface{}

var methodHandlers = map[string]rpcHandler{
	"OnQueryItem": OnQueryItem,
}

func handleRpc(stream []byte, conn net.Conn) {
	args := lua_seri.SeriUnpack(stream)
	method := args[0].(string)
	handler := methodHandlers[method]
	if handler == nil {
		panic(fmt.Sprintf("unknow rpc %s", method))
	}

	replys := handler(args[1:]...)
	body := lua_seri.SeriPack(replys...)

	sendBuffer := bytes.NewBuffer(make([]byte, len(body)+HeadLen))
	sendBuffer.Reset()

	var head [HeadLen]byte
	binary.BigEndian.PutUint32(head[:], uint32(len(body)))
	sendBuffer.Write(head[:])

	sendBuffer.Write(body)

	n, err := conn.Write(sendBuffer.Bytes())
	if err != nil {
		panic(fmt.Sprintf("conn write failed, %v", err))
	}
	if n != len(body)+HeadLen {
		panic(fmt.Sprintf("conn write len err, %d != %d", n, len(body)+HeadLen))
	}
}

func handleConn(conn net.Conn) {
	var data [1024]byte
	recvBuffer := bytes.NewBuffer(make([]byte, 128))
	recvBuffer.Reset()
	for {
		n, err := conn.Read(data[:])
		if err != nil {
			if err == io.EOF {
				log.Println("socket peer closed")
			} else {
				log.Printf("socket err: %v\n", err)
			}
			return
		}
		if n <= 0 {
			log.Println("socket peer closed")
			return
		}
		recvBuffer.Write(data[:n])
		for {
			stream := recvBuffer.Bytes()
			recvLen := len(stream)
			if recvLen < HeadLen {
				break
			}
			bodyLen := int(binary.BigEndian.Uint32(stream[:HeadLen]))
			pkgLen := bodyLen + HeadLen
			if recvLen < pkgLen {
				break
			}
			handleRpc(stream[HeadLen:pkgLen], conn)
			recvBuffer.Next(pkgLen)
		}
	}
}

func main() {
	flag.StringVar(&addr, "addr", ":5051", "listen addr")
	flag.Parse()
	lis, err := net.Listen("tcp", addr)
	if err != nil {
		panic(fmt.Sprintf("socket listen err:%v", err))
	}
	log.Printf("listen %s ok!\n", addr)
	for {
		conn, err := lis.Accept()
		if err != nil {
			panic(fmt.Sprintf("socket accpet err:%v", err))
		}
		go handleConn(conn)
	}
}
