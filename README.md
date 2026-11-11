# AgRak Android

[![Build & Release AGRak](https://github.com/RealHarD-AG/AgRakAndroid/actions/workflows/build-apk.yml/badge.svg)](https://github.com/RealHarD-AG/AgRakAndroid/actions/workflows/build-apk.yml)
[![Download Latest APK](https://img.shields.io/badge/Download-AGRak.apk-2ea44f?style=for-the-badge&logo=android&logoColor=white)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/AGRak.apk)
[![Downloads](https://img.shields.io/github/downloads/RealHarD-AG/AgRakAndroid/total?color=blue&style=for-the-badge&label=Downloads)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest)

---

## 📥 دانلود برنامه و کتابخانه‌ها (Downloads & Releases)

برای دریافت آخرین نسخه رسمی کلاینت اندروید AGRak و کتابخانه‌های بومی کامپایل‌شده می‌توانید از پیوندهای مستقیم زیر استفاده نمایید:

* 📱 **[دانلود مستقیم آخرین نسخه برنامه (AGRak.apk)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/AGRak.apk)**
* ⚙️ **[دانلود کتابخانه ۶۴ بیتی ARM (libraksamp-arm64-v8a.so)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-arm64-v8a.so)**
* ⚙️ **[دانلود کتابخانه ۳۲ بیتی ARM (libraksamp-armeabi-v7a.so)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-armeabi-v7a.so)**
* 🗜️ **[دانلود فشرده هر دو معماری (libraksamp-android.zip)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-android.zip)**
* 📋 **[مشاهده صفحه انتشارات ریلیزها (All Releases)](https://github.com/RealHarD-AG/AgRakAndroid/releases)**

---

## 🛡️ شفافیت کامل و تضمین امنیت (Open Source & Security Transparency)

جهت رفع هرگونه ابهام یا نگرانی کاربران پیرامون فایل کتابخانه بومی (`libraksamp.so`):

1. **دسترسی به تمامی سورس‌کدها:**  
   تمامی سورس‌کدهای زبان C و C++ مربوط به موتور راک‌سمپ بدون هیچ‌گونه استثنا یا پنهان‌کاری در پوشه [`RakSAMP-cli/`](./RakSAMP-cli/) همین مخزن به صورت عمومی در دسترس و قابل بررسی، بازبینی و حسابرسی (Audit) امنیتی است.

2. **کامپایل ۱۰۰٪ خودکار توسط گیت‌هاب (CI/CD):**  
   هیچ فایل اجرایی یا باینری (`.apk` یا `.so`) توسط اشخاص یا به صورت دستی در این مخزن قرار نگرفته و نخواهد گرفت. فرآیند ساخت و کامپایل تمامی فایل‌های کتابخانه بومی و پکیج نصبی APK، خط به خط توسط [GitHub Actions Workflow](.github/workflows/build-apk.yml) و بر روی سرورهای ابری گیت‌هاب انجام شده و به طور خودکار با شناسه سیستمی ربات (`github-actions[bot]`) در بخش Releases قرار می‌گیرد.

---

## 🏗️ راهنمای کامپایل محلی (Local Build)

در صورت تمایل به کامپایل دستی روی سیستم خود:

### ۱. پیش‌نیازها
* Android SDK (API 34)
* Android NDK (r25c یا بالاتر)
* CMake و Make
* JDK 17

### ۲. دستور کامپایل کتابخانه بومی و اپلیکیشن
```bash
# کامپایل کتابخانه‌های بومی (arm64-v8a و armeabi-v7a)
chmod +x build_native.sh
./build_native.sh

# کامپایل فایل نصبی APK
chmod +x gradlew
./gradlew assembleRelease
```
فایل نهایی در مسیر `app/build/outputs/apk/release/app-release.apk` تولید خواهد شد.
