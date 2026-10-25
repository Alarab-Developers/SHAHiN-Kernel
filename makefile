BUILD=build

KERNEL=$(BUILD)/kernel.elf
BIN=$(BUILD)/kernel.bin
IMAGE=شاهين.نواة

# أداة التحويل تُبنى تلقائيًا داخل مجلد البناء
CONVERTER=$(BUILD)/ادوات/المحول

# =========================
# أعلام الترجمة
# =========================
CFLAGS=-ffreestanding \
       -fno-stack-protector \
       -mno-red-zone \
       -nostdlib \
       -I. \
       -MMD -MP

NASMFLAGS=-f elf64 -g

LDFLAGS=-T ربط_الملفات.ld

# =========================
# ترتيب الربط
# =========================
OBJS=\
	$(BUILD)/النواه.o \
	$(BUILD)/الجوهرة.o \
	$(BUILD)/محرك_الفيديو.o \
	$(BUILD)/بوابة_الفيديو.o \
	$(BUILD)/الخط.o \
	$(BUILD)/شاشة_التطوير.o \
	$(BUILD)/المكتبات.o \
	$(BUILD)/بوابة_الذاكرة.o \
	$(BUILD)/محرك_الذاكرة.o \
	$(BUILD)/مدير_الكومة.o \
	$(BUILD)/مدير_الصفحات.o \
	$(BUILD)/مدير_فضاء_العناوين.o \
	$(BUILD)/الشاشة_الحرجة.o \
	$(BUILD)/مدير_الاطار.o \
	$(BUILD)/محرك_الجدولة.o \
	$(BUILD)/بوابة_الجدولة.o \
	$(BUILD)/مُدير_الأحداث.o \
	$(BUILD)/مُدير_راحة_العمليات.o \
	$(BUILD)/مُدير_قائمة_الأنتظار.o \
	$(BUILD)/بوابة_المقاطعات.o \
	$(BUILD)/مُدير_جدول_المقاطعات.o \
	$(BUILD)/مُعالج_IRQ.o \
	$(BUILD)/مدير_الطاقة.o \
	$(BUILD)/مُتحكم_المقاطعات.o \
	$(BUILD)/مُتحكم_وموجه_المقاطعات.o \
	$(BUILD)/المؤقت.o \
	$(BUILD)/بوابة_العمليات.o \
	$(BUILD)/محرك_العمليات.o \
	$(BUILD)/مدير_عمليات_النواه.o \
	$(BUILD)/مُبدل_الحالة.o \
	$(BUILD)/مُحمل_جدول_واصفات_المقاطعات.o \
	$(BUILD)/مُدير_جدول_الواصفات_العام.o \
	$(BUILD)/مُدير_محمل_جدول_الواصفات_العام.o \
	$(BUILD)/معالج_خدمات_المقاطعات.o \
	$(BUILD)/لوحة_المفاتيح.o \
	$(BUILD)/ساتا.o \
	$(BUILD)/لغة.o \
	$(BUILD)/مجمع_الترجمة.o \
	$(BUILD)/المحلل.o \
	$(BUILD)/شجرة.o \
	$(BUILD)/مترجم_اللغة.o \
	$(BUILD)/مخزن_المتغيرات.o \
	$(BUILD)/بوابة_التنفيذ.o \
	$(BUILD)/سياق_اللغة.o \
	$(BUILD)/الدوال.o \
	$(BUILD)/الحلقات.o \
	$(BUILD)/الشروط.o \
	$(BUILD)/رياضيات.o \
	$(BUILD)/الهياكل.o \
	$(BUILD)/المتغيرات.o \
	$(BUILD)/مكتبات_اللغة.o \
	$(BUILD)/التعليقات.o \
	$(BUILD)/ادوات.o \
	$(BUILD)/التصريح_بالأنواع.o \
	$(BUILD)/نظام_العرب.o \
	$(BUILD)/الملفات.o \
	$(BUILD)/التعريف_العام.o \
	$(BUILD)/التهيئة.o \
	$(BUILD)/العقدة.o \
	$(BUILD)/القرائة.o \
	$(BUILD)/الكتابة.o \
	$(BUILD)/الكتلة.o \
	$(BUILD)/المجلدات.o \
	$(BUILD)/المسارات.o \
	$(BUILD)/محمل_التطبيقات.o \
	$(BUILD)/شروط_لوحة_المفاتيح.o \
	$(BUILD)/اختبار_اول.o \
	$(BUILD)/مؤشر_الفأرة.o

# =========================
# البحث التلقائي عن الملفات المصدرية
# (مع استبعاد مجلد البناء ومجلد أدوات)
# =========================
CSRCS := $(shell find . -path './$(BUILD)' -prune -o -path './أدوات' -prune -o -name '*.c' -print)
ASRCS := $(shell find . -path './$(BUILD)' -prune -o -path './أدوات' -prune -o -name '*.asm' -print)

vpath %.c   $(sort $(dir $(CSRCS)))
vpath %.asm $(sort $(dir $(ASRCS)))

all: $(IMAGE)

# =========================
# إنشاء مجلدات البناء
# =========================
$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/ادوات:
	mkdir -p $@

# =========================
# قواعد الترجمة العامة
# =========================
$(BUILD)/%.o: %.c | $(BUILD)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm | $(BUILD)
	nasm $(NASMFLAGS) $< -o $@

# =========================
# بناء أداة التحويل (المحول)
# freestanding — بدون libc، تعتمد على مكتبة النواة فقط
# =========================
CONVERTER_SRCS = ادوات/المحول.c القلب/المكتبات/المكتبات.c
CONVERTER_INC  = -Iالقلب/المكتبات

$(CONVERTER): $(CONVERTER_SRCS) \
              القلب/المكتبات/المكتبات.h \
              القلب/المكتبات/انواع.h   \
              | $(BUILD)/ادوات
	@echo "=== بناء أداة التحويل (freestanding) ==="
	gcc -nostdlib -ffreestanding -fno-builtin \
	    -fno-stack-protector -fno-pic -no-pie \
	    -mno-red-zone -O2 -Wall -Wextra \
	    $(CONVERTER_INC) \
	    -o $@ $(CONVERTER_SRCS)
	@echo "[✓] $(CONVERTER)"

# =========================
# الربط
# =========================
$(KERNEL): $(OBJS) ربط_الملفات.ld
	@echo "=== بناء النواة ==="

	gcc $(CFLAGS) \
		$(LDFLAGS) \
		-o $(KERNEL) \
		$(OBJS)

	@echo "[✓] $(KERNEL)"

	@echo "=== Symbols المهمة ==="
	@nm $(KERNEL) | grep -E "KernelMain|_kernel_start|_kernel_end|_bss_start|_bss_end" || true

	@KERNEL_MAIN_ADDR=$$(nm $(KERNEL) | awk '$$3=="KernelMain" {print "0x"$$1}'); \
	echo "[INFO] KernelMain @ $$KERNEL_MAIN_ADDR"

# =========================
# kernel.bin
# =========================
$(BIN): $(KERNEL)
	objcopy -O binary $(KERNEL) $(BIN)
	@echo "[✓] $(BIN) — $$(wc -c < $(BIN)) bytes"

# =========================
# شاهين.نواة — يُنتَج باستخدام المحول
# =========================
$(IMAGE): $(BIN) $(CONVERTER)
	@echo "=== استخراج العناوين من $(KERNEL) ==="
	@KERNEL_MAIN_ADDR=$$(nm $(KERNEL) | awk '$$3=="KernelMain" {print "0x"$$1}'); \
	KERNEL_END_ADDR=$$(nm $(KERNEL)  | awk '$$3=="_kernel_end" {print "0x"$$1}'); \
	BSS_END_ADDR=$$(nm $(KERNEL)     | awk '$$3=="_bss_end"    {print "0x"$$1}'); \
	if [ -z "$$KERNEL_MAIN_ADDR" ] || [ -z "$$KERNEL_END_ADDR" ]; then \
		echo "ERROR: لم أستطع استخراج KernelMain أو _kernel_end من $(KERNEL)"; \
		exit 1; \
	fi; \
	echo "[INFO] KernelMain  = $$KERNEL_MAIN_ADDR"; \
	echo "[INFO] _kernel_end = $$KERNEL_END_ADDR"; \
	echo "[INFO] _bss_end    = $$BSS_END_ADDR"; \
	\
	LOAD_BASE=$$(printf "%d" 0x100000); \
	KEND_DEC=$$(printf "%d" $$KERNEL_END_ADDR); \
	SIZE=$$((KEND_DEC - LOAD_BASE)); \
	echo "[INFO] image_size (قبل المحول) = $$SIZE bytes (0x$$(printf '%x' $$SIZE))"; \
	\
	cp -f $(BIN) kernel.bin; \
	./$(CONVERTER) $$KERNEL_MAIN_ADDR $$KERNEL_END_ADDR; \
	rm -f kernel.bin

	@echo "[✓] $(IMAGE)"
	@ls -la $(IMAGE)

	@echo "Removing object files..."
	@rm -f $(OBJS)

# =========================
# تنظيف
# =========================
clean:
	rm -rf $(BUILD) $(IMAGE) kernel.bin

.PHONY: all clean

-include $(wildcard $(BUILD)/*.d)

# =========================
# أداة arpc (تعمل على المضيف Linux — ليست للنواة)
# =========================
HOST_CC   = gcc
ARPC_DIR  = ادوات
ARPC      = $(BUILD)/arpc

# ملاحظة: لا نضمّن سياق_اللغة.c — host_compat.c يوفّر بديلًا عنه
ARPC_SRCS = \
	$(ARPC_DIR)/arpc.c \
	$(ARPC_DIR)/host_compat.c \
	القلب/الوحدات/اللغة/لغة.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/المحلل.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/مترجم_اللغة.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/مجمع_الترجمة.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/شجرة.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/مخزن_المتغيرات.c \
	القلب/الوحدات/اللغة/طبقة_الترجمة/التصريح_بالأنواع.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/الدوال.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/المتغيرات.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/الهياكل.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/التعليقات.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/رياضيات.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/مكتبات_اللغة.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/الحلقات.c \
	القلب/الوحدات/اللغة/مفاهيم_اللغة/الشروط.c \
	القلب/الوحدات/اللغة/ادوات/ادوات.c

$(ARPC): $(ARPC_SRCS) | $(BUILD)
	@echo "=== بناء arpc (مترجم المضيف) ==="
	$(HOST_CC) -O2 -Wall -I. -o $@ $(ARPC_SRCS)
	@echo "[✓] $(ARPC)"

# =========================
# ترجمة التطبيقات إلى .arpe
# =========================
APPS_SRC  = $(wildcard *.تطبيق)
APPS_ARPE = $(patsubst %.تطبيق,$(BUILD)/%.arpe,$(APPS_SRC))

arpe: $(ARPC) $(APPS_ARPE)

$(BUILD)/%.arpe: %.تطبيق $(ARPC) | $(BUILD)
	$(ARPC) $< -o $@

.PHONY: arpe
