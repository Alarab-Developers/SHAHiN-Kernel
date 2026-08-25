BUILD=build

KERNEL=$(BUILD)/kernel.elf
BIN=$(BUILD)/kernel.bin

# إضافة -I. لجعل المجلد الرئيسي هو جذر التضمين
CFLAGS=-ffreestanding \
       -fno-stack-protector \
       -mno-red-zone \
       -nostdlib \
       -I. \
       -MMD -MP

NASMFLAGS=-f elf64 -g

LDFLAGS=-T ربط_الملفات.ld

# =========================
# ترتيب الربط (مهم جدًا لإقلاع النواة!)
# نفس ترتيبك الأصلي بالضبط — النواه.o يجب أن يبقى أولًا
# =========================
OBJS=\
	$(BUILD)/النواه.o \
	$(BUILD)/الجوهرة.o \
	$(BUILD)/مدير_الوحدات.o \
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
	$(BUILD)/محرك_تواصل_العمليات.o \
	$(BUILD)/مُبدل_الحالة.o \
	$(BUILD)/مُحمل_جدول_واصفات_المقاطعات.o \
	$(BUILD)/مُدير_جدول_الواصفات_العام.o \
	$(BUILD)/مُدير_محمل_جدول_الواصفات_العام.o \
	$(BUILD)/معالج_خدمات_المقاطعات.o \
	$(BUILD)/لوحة_المفاتيح.o \
	$(BUILD)/ساتا.o \
	$(BUILD)/لغة.o \
	$(BUILD)/المحلل.o \
	$(BUILD)/المنفذ.o \
	$(BUILD)/الدوال.o \
	$(BUILD)/الحلقات.o \
	$(BUILD)/الشروط.o \
	$(BUILD)/رياضيات.o \
	$(BUILD)/المتغيرات.o \
	$(BUILD)/مكتبات_اللغة.o \
	$(BUILD)/ادوات.o \
	$(BUILD)/اوامر_اللغة.o \
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
	$(BUILD)/اختبار_اول.o \
	$(BUILD)/مؤشر_الفأرة.o

# =========================
# جعل make يجد الملفات المصدرية بغض النظر عن مكانها
# (بدل كتابة المسار الكامل في كل قاعدة)
# =========================
CSRCS := $(shell find . -path './$(BUILD)' -prune -o -name '*.c' -print)
ASRCS := $(shell find . -path './$(BUILD)' -prune -o -name '*.asm' -print)

vpath %.c   $(sort $(dir $(CSRCS)))
vpath %.asm $(sort $(dir $(ASRCS)))

all: $(BIN)

# =========================
# إنشاء مجلد البناء
# =========================

$(BUILD):
	mkdir -p $(BUILD)

# =========================
# قواعد عامة للترجمة (بدل عشرات القواعد المكررة)
# الاعتماديات على ملفات .h تُولَّد تلقائيًا عبر -MMD -MP
# =========================

$(BUILD)/%.o: %.c | $(BUILD)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm | $(BUILD)
	nasm $(NASMFLAGS) $< -o $@

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

	@echo "=== Symbols ==="
	@nm $(KERNEL) | grep -E "KernelMain|__bss" || true

	@KERNEL_MAIN_ADDR=$$(nm $(KERNEL) | grep " T KernelMain" | awk '{print "0x"$$1}'); \
	echo "[INFO] KernelMain @ $$KERNEL_MAIN_ADDR"

# =========================
# kernel.bin
# =========================

$(BIN): $(KERNEL)
	objcopy -O binary $(KERNEL) $(BIN)
	@echo "[✓] $(BIN) — $$(wc -c < $(BIN)) bytes"

	@echo "Removing object files..."
	@rm -f $(OBJS)

# =========================
# تنظيف
# =========================

clean:
	rm -rf $(BUILD)

.PHONY: all clean

# اعتماديات ملفات .h المولّدة تلقائيًا
-include $(wildcard $(BUILD)/*.d)
