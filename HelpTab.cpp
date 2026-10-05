#include "HelpTab.h"
#include "MainWindow.h"
#include "ZeroTierClient.h"
#include "DiagnosticReport.h"
#include <QDateTime>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

HelpTab::HelpTab(MainWindow *mainWindow, QWidget *parent) : QWidget(parent), m_main(mainWindow)
{
    auto *layout = new QVBoxLayout(this);
    m_content = new QTextBrowser;
    m_content->setObjectName("helpContent");
    m_content->setOpenExternalLinks(true);
    m_content->setHtml(QStringLiteral(
        "<h2>ZeroTier GUI</h2><p>项目源码与反馈："
        "<a href='https://github.com/vusiif/zerotier-gui'>GitHub</a> · "
        "<a href='https://gitee.com/vusiif/zerotier-gui'>Gitee</a></p>"
        "<p>网络：输入16位网络 ID 加入；私有网络需要管理员在控制台授权。"
        "选中网络后可查询属性/IP，修改托管地址、默认路由、全局地址和 DNS。</p>"
        "<p>Moon：可订阅 world ID 与 seed 节点，或导入签名 .moon 文件到服务实际数据目录。"
        "导入后重启服务会短暂断开连接。</p>"
        "<p>列表静默刷新，名称仅保存在本机；详细信息浮窗保留完整 JSON。"
        "右上角切换主题或置顶，汉堡按钮收起侧栏。</p>"
        "<p>诊断导出可能包含 IP、节点标识和网络配置，请在分享前检查文件。"
        "操作日志只写入本地单个文件，不自动上传。</p>"));
    layout->addWidget(m_content, 1);
    auto *actions = new QHBoxLayout;
    auto *help = new QPushButton(QStringLiteral("CLI 用法"));
    auto *version = new QPushButton(QStringLiteral("CLI 版本"));
    auto *dump = new QPushButton(QStringLiteral("导出诊断…"));
    dump->setObjectName("exportDiagnosticsButton");
    actions->addWidget(help); actions->addWidget(version); actions->addWidget(dump); actions->addStretch();
    layout->addLayout(actions);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    connect(help, &QPushButton::clicked, this, [this] { query("-h"); });
    connect(version, &QPushButton::clicked, this, [this] { query("-v"); });
    connect(dump, &QPushButton::clicked, this, &HelpTab::exportDiagnostics);
}

void HelpTab::query(const QString &argument)
{
    if (m_busy || m_main->operationBusy()) return;
    m_busy = true;
    ZeroTier::command(this, {argument}, [this](bool ok, const QString &output) {
        m_busy = false;
        if (ok) {
            auto *dialog = new QDialog(this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->setWindowTitle(QStringLiteral("ZeroTier CLI"));
            dialog->resize(640, 480);
            auto *layout = new QVBoxLayout(dialog);
            auto *text = new QTextBrowser;
            text->setPlainText(output);
            layout->addWidget(text);
            dialog->show();
        } else m_status->setText(output);
    });
}

void HelpTab::exportDiagnostics()
{
    if (m_busy || m_main->operationBusy()) return;
    const auto path = QFileDialog::getSaveFileName(this, QStringLiteral("保存诊断"), "zerotier-diagnostics.txt", "Text (*.txt)");
    if (path.isEmpty()) return;
    if (QMessageBox::question(this, QStringLiteral("导出诊断"),
        QStringLiteral("报告可能包含节点标识、IP 和网络配置；CLI 也可能更新本地 zerotier_dump.txt。确定生成并保存？"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!m_main->beginOperation()) return;
    m_busy = true;
    m_status->setText(QStringLiteral("正在生成诊断…"));
    const auto started = QDateTime::currentMSecsSinceEpoch();
    ZeroTier::command(this, {"dump"}, [this, path, started](bool ok, const QString &output) {
        m_busy = false;
        m_main->endOperation();
        if (!ok) { m_status->setText(QStringLiteral("诊断未完成：%1").arg(output)); return; }
        const auto report = DiagnosticReport::read(output.toUtf8(), started);
        QString error = report.error;
        if (error.isEmpty() && DiagnosticReport::save(path, report.text, &error))
            m_status->setText(QStringLiteral("诊断已保存：%1").arg(path));
        else m_status->setText(QStringLiteral("导出失败：%1").arg(error));
    });
}
