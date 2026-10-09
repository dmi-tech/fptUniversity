# Driver Timer – Hẹn giờ, chạy định kỳ, đo thời gian

> Làm nhiều việc với chu kỳ khác nhau mà **không** dùng `k_msleep()` trong vòng lặp chính.

---

## 1. Phần cứng

Không cần đấu dây.

| Chức năng | Dùng gì | Độ phân giải |
| --- | --- | --- |
| Hẹn giờ định kỳ / một lần | `k_timer` (phần mềm, dựa trên SysTick) | 1 ms |
| Đồng hồ bấm giờ (stopwatch) | Bộ đếm chu kỳ CPU (`k_cycle_get_32`) | 4 ns (250 MHz) |
| Hẹn giờ phần cứng (nâng cao) | TIM5 32 bit, đếm 1 MHz | 1 µs |

## 2. Xung đột tài nguyên

| Tài nguyên | Ghi chú |
| --- | --- |
| TIM5 | Chỉ phần **nâng cao**. TIM5 có kênh trên PA2/PA3 nhưng ở chế độ đếm **không dùng chân**, nên không đụng ADC/motor |

---

## 3. Cấu hình

### Phần cơ bản (hẹn giờ + đồng hồ bấm giờ)

**Bước 1 – `prj.conf`:** không cần thêm gì (`k_timer` luôn có).

**Bước 2 – `app.overlay`:** không cần.

**Bước 3 – `CMakeLists.txt`**
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/timer
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/timer/driver_timer.c
)
```

### Phần nâng cao (hẹn giờ phần cứng TIM5) – thêm vào phần cơ bản

`prj.conf`
```
CONFIG_COUNTER=y
```

`app.overlay`
```dts
/ {
	aliases { hwtimer0 = &counter5; };
};

&timers5 {
	st,prescaler = <249>;     /* 250 MHz / (249 + 1) = 1 MHz → mỗi tick 1 µs */
	status = "okay";
	counter5: counter {
		status = "okay";
	};
};
```
Giải thích: `st,prescaler` chia tần số clock của timer. TIM5 là bộ đếm 32 bit nên ở 1 MHz đếm được
tới ~71 phút trước khi tràn.

---

## 4. API (`driver_timer.h`)

### 4.1 Hẹn giờ phần mềm
```c
typedef void (*driver_timer_cb_t)(void *user_data);

struct driver_timer;   /* khai báo biến kiểu này, mỗi biến là một bộ hẹn giờ */
```

| Hàm | Mô tả |
| --- | --- |
| `int driver_timer_start(struct driver_timer *t, uint32_t period_ms, bool periodic, driver_timer_cb_t cb, void *user_data)` | Bắt đầu. `periodic = true` → lặp lại; `false` → chạy 1 lần |
| `int driver_timer_stop(struct driver_timer *t)` | Dừng |
| `bool driver_timer_is_running(const struct driver_timer *t)` | Đang chạy? |

**Callback chạy trong system workqueue (thread), không chạy trong ngắt** → được phép `printk`,
đọc cảm biến I2C, ghi LCD. Không nên chặn lâu hơn chu kỳ của timer.

### 4.2 Đồng hồ bấm giờ
| Hàm | Mô tả |
| --- | --- |
| `void driver_timer_stopwatch_start(struct driver_stopwatch *sw)` | Ghi mốc bắt đầu |
| `uint32_t driver_timer_stopwatch_us(const struct driver_stopwatch *sw)` | Số µs đã trôi qua (tối đa ~17 giây) |

### 4.3 Hẹn giờ phần cứng (nâng cao)
| Hàm | Mô tả |
| --- | --- |
| `int driver_timer_hw_init(void)` | Khởi tạo TIM5 (alias `hwtimer0`) |
| `int driver_timer_hw_alarm_us(uint32_t us, driver_timer_cb_t cb, void *user_data)` | Gọi `cb` sau `us` micro-giây, **một lần** |
| `uint32_t driver_timer_hw_now_us(void)` | Giá trị bộ đếm hiện tại (µs) |

⚠ Callback của hẹn giờ phần cứng chạy **trong ngắt**: **không** `printk` dài, **không** gọi
`k_msleep`, I2C, SPI. Chỉ đặt cờ, đảo GPIO hoặc `k_sem_give()`.

---

## 5. Ví dụ `src/main.c` (phần cơ bản)

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_timer.h"

static struct driver_timer fast_timer;
static struct driver_timer slow_timer;
static struct driver_timer once_timer;

static void on_fast(void *user_data)
{
	static uint32_t count;

	printk("[200 ms] lan thu %u\n", ++count);
}

static void on_slow(void *user_data)
{
	printk("[1 s] uptime = %lld ms\n", k_uptime_get());
}

static void on_once(void *user_data)
{
	printk("[5 s] Chay 1 lan: %s\n", (const char *)user_data);
	driver_timer_stop(&fast_timer);   /* sau 5 giây dừng timer nhanh */
}

int main(void)
{
	struct driver_stopwatch sw;

	driver_timer_start(&fast_timer, 200, true, on_fast, NULL);
	driver_timer_start(&slow_timer, 1000, true, on_slow, NULL);
	driver_timer_start(&once_timer, 5000, false, on_once, "dung timer nhanh");

	/* Đo thời gian thực thi một đoạn code */
	driver_timer_stopwatch_start(&sw);
	k_busy_wait(1234);
	printk("k_busy_wait(1234) mat %u us\n", driver_timer_stopwatch_us(&sw));

	/* main không cần làm gì nữa – các timer tự chạy */
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
k_busy_wait(1234) mat 1234 us
[200 ms] lan thu 1
...
[1 s] uptime = 1003 ms
...
[5 s] Chay 1 lan: dung timer nhanh
[1 s] uptime = 6003 ms
```

---

## 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Callback chạy trễ, chu kỳ không đều | Một callback khác chặn workqueue quá lâu | Rút ngắn callback, đẩy việc nặng sang thread riêng |
| Biến `struct driver_timer` khai báo trong hàm, timer chạy vài lần rồi treo | Biến cục bộ bị hủy khi hàm kết thúc | Khai báo `static` hoặc toàn cục |
| Hard fault khi dùng hẹn giờ phần cứng | Gọi hàm chặn (`printk` dài, I2C) trong callback ngắt | Chỉ đặt cờ / `k_sem_give()` trong callback |
| `driver_timer_hw_init` trả `-19` | Thiếu overlay `&timers5` hoặc `CONFIG_COUNTER` | Xem mục 3 phần nâng cao |

## 8. Tham khảo
- Zephyr Timers: https://docs.zephyrproject.org/latest/kernel/services/timing/timers.html
- Zephyr Workqueue: https://docs.zephyrproject.org/latest/kernel/services/threads/workqueue.html
- Zephyr Counter API: https://docs.zephyrproject.org/latest/hardware/peripherals/counter.html
