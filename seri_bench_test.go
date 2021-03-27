package lua_seri

import (
	"encoding/json"
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
)

var jsonItems = map[string]interface{}{
	"1": map[string]interface{}{
		"ID":    int64(210001),
		"Count": int64(100),
	},
	"2": map[string]interface{}{
		"ID":    int64(310001),
		"Count": int64(200),
	},
	"3": map[string]interface{}{
		"ID":    int64(520001),
		"Count": 308.78,
	},
	"4": map[string]interface{}{
		"ID":    int64(620001),
		"Count": int64(400),
	},
	"5": map[string]interface{}{
		"ID":    int64(720001),
		"Count": 542.192,
	},
	"6": map[string]interface{}{
		"ID":    int64(820001),
		"Count": int64(600),
	},
	"Diamond": int64(800),
	"Gold":    999.99,
	"Copper": map[string]interface{}{
		"Owner":   "qwerty",
		"Price":   666.76,
		"Useable": true,
	},
	"Trader":    "lakefu",
	"Livehouse": "mount nan",
	"Deadline":  "2024-01-07 09:42:20",
}

var tableItems = &Table{
	Array: []interface{}{
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(210001),
				"Count": int64(100),
			},
		},
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(310001),
				"Count": int64(200),
			},
		},
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(520001),
				"Count": 308.78,
			},
		},
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(620001),
				"Count": int64(400),
			},
		},
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(720001),
				"Count": 542.192,
			},
		},
		&Table{
			Hashmap: map[interface{}]interface{}{
				"ID":    int64(820001),
				"Count": int64(600),
			},
		},
	},
	Hashmap: map[interface{}]interface{}{
		"Diamond": int64(800),
		"Gold":    999.99,
		"Copper": &Table{
			Hashmap: map[interface{}]interface{}{
				"Owner":   "qwerty",
				"Price":   666.76,
				"Useable": true,
			},
		},

		"Trader":    "lakefu",
		"Livehouse": "mount nan",
		"Deadline":  "2024-01-07 09:42:20",
	},
}

func BenchmarkLuaSeri(b *testing.B) {
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		buffer := SeriPack(tableItems)
		SeriUnpack(buffer)
	}
}

func BenchmarkJson(b *testing.B) {
	b.ResetTimer()
	var v interface{}
	for i := 0; i < b.N; i++ {
		buffer, _ := json.Marshal(jsonItems)
		json.Unmarshal(buffer, &v)
	}
}

var isExecuted = false

func BenchmarkPackageLen(b *testing.B) {
	if isExecuted {
		return
	}
	isExecuted = true
	tableBuffer := SeriPack(tableItems)
	fmt.Printf("LuaSeri pkg len:%d\n", len(tableBuffer))
	jsonBuffer, err := json.Marshal(jsonItems)
	assert.Nil(b, err)
	fmt.Printf("Json    pkg len:%d\n", len(jsonBuffer))
}
