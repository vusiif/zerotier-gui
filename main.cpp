#include <QApplication>
#include "MainWindow.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <securitybaseapi.h>

static bool isRunningAsAdmin()
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

static bool relaunchAsAdmin()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // 拼接命令行参数
    QString args;
    QStringList argList = QCoreApplication::arguments();
    for (int i = 1; i < argList.size(); ++i) {
        if (!args.isEmpty()) args += ' ';
        args += '"' + argList[i] + '"';
    }

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_DEFAULT;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.isEmpty() ? nullptr : (LPCWSTR)args.utf16();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei);
}
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 1. 检查管理员权限
    if (!isRunningAsAdmin()) {
        // 2. 尝试提权重启
        if (relaunchAsAdmin()) {
            return 0;  // 提权成功，退出当前进程
        }
        // 3. 用户拒绝 UAC → 弹出提示后退出
        MessageBoxW(nullptr,
            L"此程序需要管理员权限才能管理 ZeroTier 服务，请同意 UAC 提权请求。",
            L"ZeroTier GUI", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    // 4. 已经是管理员，继续正常启动
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ZeroTier GUI");

    MainWindow window;
    window.show();

    return app.exec();
}
