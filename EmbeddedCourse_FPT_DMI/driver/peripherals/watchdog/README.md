# Driver Watchdog – Tự khởi động lại khi chương trình bị treo

> Nếu chương trình không "cho ăn" (feed) watchdog trong thời gian quy định, chip **tự reset**.
> Đây là cơ chế an toàn bắt buộc trong sản phẩm thật.

---

## 1. Phần cứng

Không cần đấu dây. IWDG chạy bằng dao động nội LSI (~32 kHz), độc lập với clock chính, nên vẫn
reset được kể cả khi clock chính hỏng.

| Thông số | Giá trị |
| --- | --- |
| Timeout tối thiểu | ~1 ms |
| Timeout tối đa | ~32 giây (giới hạn của driver này; phần cứng cho phép lâu hơn) |
| Dừng được sau khi đã bật? | **Không** – chỉ reset mới tắt được |

## 2. Xung đột tài nguyên

Không có. Lưu ý: khi dừng CPU bằng debugger, driver đã cấu hình để watchdog **tạm dừng theo**
(không reset khi bạn đang debug).

---

## 3. Cấu hình

### Bước 1 – `prj.conf`
```
CONFIG_WATCHDOG=y
CONFIG_HWINFO=y
```
- `CONFIG_WATCHDOG`: driver watchdog của Zephyr.
- `CONFIG_HWINFO`: đọc nguyên nhân reset (để biết lần khởi động trước có phải do watchdog không).

### Bước 2 – `app.overlay`
```dts
/ {
	aliases { watchdog0 = &iwdg; };
};

&iwdg { status = "okay"; };
```

### Bước 3 – `CMakeLists.txt`
```cmake
target_include_directories(app PRIVATE
  ${DRIVER_DIR}/peripherals/watchdog
)
target_sources(app PRIVATE
  ${DRIVER_DIR}/peripherals/watchdog/driver_watchdog.c
)
```

---

## 4. API (`driver_watchdog.h`)

| Hàm | Mô tả | Trả về |
| --- | --- | --- |
| `int driver_watchdog_init(uint32_t timeout_ms)` | Cấu hình **và bật** watchdog. Gọi **một lần** | 0 / `-EINVAL` nếu timeout ngoài 1..32000 / `-ENODEV` |
| `int driver_watchdog_feed(void)` | "Cho ăn" – phải gọi thường xuyên hơn `timeout_ms` | 0 / lỗi âm |
| `bool driver_watchdog_caused_reset(void)` | `true` nếu lần reset vừa rồi do watchdog. Gọi **đầu** `main()` | – |

Quy tắc chọn timeout: **gấp 2–5 lần** khoảng thời gian feed dài nhất trong chương trình.

---

## 5. Ví dụ `src/main.c`

Chương trình feed watchdog mỗi 500 ms. Sau 10 lần, giả lập "treo" bằng vòng lặp vô hạn → sau
2 giây chip tự reset → lần khởi động sau in thông báo.

```c
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "driver_watchdog.h"

int main(void)
{
	int ret;

	if (driver_watchdog_caused_reset()) {
		printk("!!! Lan khoi dong truoc bi WATCHDOG reset !!!\n");
	} else {
		printk("Khoi dong binh thuong\n");
	}

	ret = driver_watchdog_init(2000);   /* 2 giây */
	if (ret < 0) {
		printk("Loi watchdog: %d\n", ret);
		return 0;
	}

	for (int i = 1; i <= 10; i++) {
		driver_watchdog_feed();
		printk("Feed lan %d\n", i);
		k_msleep(500);
	}

	printk("Gia lap treo... (cho 2 giay)\n");
	while (1) {
		/* không feed nữa → watchdog reset */
	}
	return 0;
}
```

## 6. Kết quả mong đợi (RTT)
```text
Khoi dong binh thuong
Feed lan 1
...
Feed lan 10
Gia lap treo... (cho 2 giay)
*** Booting MCUboot ...        (trên UART5)
!!! Lan khoi dong truoc bi WATCHDOG reset !!!
Feed lan 1
...
```

---

## 7. Thiết kế đúng – đọc kỹ

- **Chỉ feed ở một chỗ**, nơi chứng minh được chương trình đang chạy đúng (ví dụ cuối vòng lặp
  chính, sau khi mọi tác vụ đã báo "còn sống"). **Không** feed trong timer định kỳ: timer vẫn
  chạy dù vòng lặp chính đã treo, nên watchdog trở nên vô dụng.
- Khi có nhiều thread: mỗi thread đặt một cờ "còn sống"; vòng lặp chính chỉ feed khi **tất cả**
  cờ đã được đặt, rồi xóa cờ.
- Watchdog đã bật thì **không tắt được**. Nếu bạn nạp chương trình mới không có watchdog, chip
  sẽ được reset khi nạp nên không ảnh hưởng.

## 8. Lỗi thường gặp

| Hiện tượng | Nguyên nhân | Cách sửa |
| --- | --- | --- |
| Board reset liên tục | Timeout quá ngắn so với thời gian giữa 2 lần feed (ví dụ đang chờ DHCP 10 s) | Tăng timeout hoặc feed trong lúc chờ |
| `-ENODEV` | Thiếu `&iwdg { status = "okay"; }` hoặc alias | Xem mục 3 |
| `driver_watchdog_caused_reset()` luôn `false` | Thiếu `CONFIG_HWINFO=y` | Thêm vào `prj.conf` |

## 9. Tham khảo
- Zephyr Watchdog API: https://docs.zephyrproject.org/latest/hardware/peripherals/watchdog.html
- RM0481 (Reference manual STM32H5), chương IWDG
