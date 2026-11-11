# AgRak Android

[![Build & Release AGRak](https://github.com/RealHarD-AG/AgRakAndroid/actions/workflows/build-apk.yml/badge.svg)](https://github.com/RealHarD-AG/AgRakAndroid/actions/workflows/build-apk.yml)
[![Download Latest APK](https://img.shields.io/badge/Download-AGRak.apk-2ea44f?style=for-the-badge&logo=android&logoColor=white)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/AGRak.apk)
[![Downloads](https://img.shields.io/github/downloads/RealHarD-AG/AgRakAndroid/total?color=blue&style=for-the-badge&label=Downloads)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest)

---

<div dir="rtl">

## 📥 دانلود برنامه و کتابخانه‌ها

برای دریافت آخرین نسخه رسمی کلاینت اندروید AGRak و کتابخانه‌های بومی می‌توانید از لینک‌های مستقیم زیر استفاده کنید:

* 📱 **[دانلود مستقیم آخرین نسخه برنامه (AGRak.apk)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/AGRak.apk)**
* ⚙️ **[دانلود کتابخانه ۶۴ بیتی (libraksamp-arm64-v8a.so)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-arm64-v8a.so)**
* ⚙️ **[دانلود کتابخانه ۳۲ بیتی (libraksamp-armeabi-v7a.so)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-armeabi-v7a.so)**
* 🗜️ **[دانلود فشرده هر دو کتابخانه (libraksamp-android.zip)](https://github.com/RealHarD-AG/AgRakAndroid/releases/latest/download/libraksamp-android.zip)**
* 📋 **[مشاهده صفحه انتشارات (Releases)](https://github.com/RealHarD-AG/AgRakAndroid/releases)**

---

## 🛡️ شفافیت کامل و تضمین امنیت سورس‌کد

جهت رفع هرگونه ابهام یا نگرانی کاربران پیرامون فایل کتابخانه بومی (`libraksamp.so`):

1. **سورس‌کد کامل و عمومی:** تمامی سورس‌کدهای زبان C و C++ موتور راک‌سمپ بدون استثنا در پوشه [`RakSAMP-cli/`](./RakSAMP-cli/) قرار دارند و برای عموم قابل بررسی و ارزیابی است.
2. **کامپایل خودکار با سرور گیت‌هاب (CI/CD):** هیچ فایل باینری (`.apk` یا `.so`) به شکل دستی آپلود نمی‌شود. کلیه فایل‌ها توسط [GitHub Actions Workflow](.github/workflows/build-apk.yml) از صفر کامپایل و توسط ربات رسمی گیت‌هاب (`github-actions[bot]`) در بخش Releases قرار می‌گیرند.

---

## 🏗️ راهنمای کامپایل دستی

</div>

```bash
# کامپایل کتابخانه‌های بومی (arm64-v8a و armeabi-v7a)
chmod +x build_native.sh
./build_native.sh

# کامپایل فایل نصبی APK
chmod +x gradlew
./gradlew assembleRelease
```

<div dir="rtl">
فایل خروجی در مسیر <code>app/build/outputs/apk/release/app-release.apk</code> ساخته می‌شود.
</div>
