#include "../AppStyle.h"
#include "../AppLog.h"
#include "../DataTable.h"
#include "../MainWindow.h"
#include "../ZeroTierClient.h"
#include "../tabs/NetworkTab.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTextStream>
#include <QToolButton>
#include <QTextEdit>
#include <QtTest>

class GuiTests : public QObject {
    Q_OBJECT
private:
    QTemporaryDir m_settings;
    QByteArray m_oldPath;
    static QList<TableRow> rows(int count = 60) {
        QList<TableRow> result;
        for (int i = 0; i < count; ++i) {
            const auto id = QString("%1").arg(i, 16, 16, QChar('0'));
            result.append({id, {id, "已连接", QString("10.0.0.%1").arg(i)},
                {{"id", id}, {"routes", QJsonArray{QJsonObject{{"target", "10.0.0.0/24"}}}}, {"unknown", 42}}});
        }
        return result;
    }
private slots:
    void initTestCase() {
        QVERIFY(m_settings.isValid());
        QCoreApplication::setOrganizationName("ZeroTierGUI-Tests");
        QCoreApplication::setApplicationName("RegressionTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
        m_oldPath = qgetenv("PATH");
        qputenv("PATH", QCoreApplication::applicationDirPath().toLocal8Bit() + ';' + m_oldPath);
        QVERIFY(ZeroTier::executable().startsWith(QCoreApplication::applicationDirPath()));
        applyAppStyle(*qApp);
    }
    void cleanup() {
        setAppDarkTheme(*qApp, false);
        qunsetenv("ZT_TEST_MODE");
        qunsetenv("ZT_TEST_WINGET_CODE");
        QSettings().clear();
    }
    void cleanupTestCase() { qputenv("PATH", m_oldPath); }
    void windowControlsAndFileLog() {
        MainWindow window;
        window.show();
        QTest::qWait(50);
        auto *theme = window.findChild<QToolButton *>("themeToggle");
        auto *sidebar = window.findChild<QToolButton *>("sidebarToggle");
        auto *pin = window.findChild<QToolButton *>("alwaysOnTopToggle");
        auto *navigation = window.findChild<QListWidget *>("navigation");
        auto *splitter = window.findChild<QSplitter *>("mainSplitter");
        QVERIFY(theme && sidebar && pin && navigation && splitter);
        theme->click();
        QVERIFY(qApp->property("darkTheme").toBool());
        QVERIFY(qApp->palette().color(QPalette::Text).lightness() > 180);
        QCOMPARE(QSettings().value("appearance/dark").toBool(), true);
        QVERIFY(QDir().mkpath("screenshots"));
        QVERIFY(window.grab().save("screenshots/dark-restored.png"));
        QMessageBox dialog(QMessageBox::Question, "Test", "Text", QMessageBox::Yes | QMessageBox::No);
        QVERIFY(dialog.button(QMessageBox::Yes)->palette().color(QPalette::ButtonText).lightness() > 180);
        sidebar->click();
        QVERIFY(navigation->item(0)->text().isEmpty());
        for (int row = 0; row < navigation->count(); ++row) {
            QVERIFY(!navigation->item(row)->icon().isNull());
            QVERIFY(!navigation->item(row)->toolTip().isEmpty());
        }
        QTRY_VERIFY(splitter->sizes()[0] <= 64);
        QVERIFY(window.grab().save("screenshots/collapsed-restored.png"));
        navigation->setCurrentRow(2);
        QCOMPARE(navigation->currentRow(), 2);
        sidebar->click();
        QVERIFY(!navigation->item(0)->text().isEmpty());
        QVERIFY(splitter->sizes()[0] >= 130);
        pin->click();
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(window.isVisible());
        pin->click();
        QVERIFY(!window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(!window.findChild<QTextEdit *>("outputConsole"));
        window.appendOutput("file-log-marker");
        QFile log(AppLog::path());
        QVERIFY(log.open(QIODevice::ReadOnly));
        QVERIFY(log.readAll().contains("file-log-marker"));
        log.close();
        for (int i = 0; i < 6; ++i) window.appendOutput(QString(300000, 'x'));
        QVERIFY(QFileInfo(AppLog::path()).size() <= 1024 * 1024);
        window.close();
    }

    void reconcilePreservesInteraction() {
        DataTable table("test", {"名称", "ID", "状态", "IP"});
        table.resize(620, 360);
        table.show();
        auto snapshot = rows();
        table.setRows(snapshot);
        auto *tree = table.tree();
        QTest::qWait(50);
        auto *selected = tree->topLevelItem(20);
        tree->setCurrentItem(selected);
        selected->setSelected(true);
        const QString selectedId = table.selectedId();
        tree->header()->moveSection(3, 2);
        tree->setColumnWidth(0, 235);
        tree->horizontalScrollBar()->setValue(80);
        tree->verticalScrollBar()->setValue(320);
        const int horizontal = tree->horizontalScrollBar()->value();
        const int vertical = tree->verticalScrollBar()->value();
        snapshot[20].cells[1] = "等待授权";
        snapshot.removeLast();
        snapshot.prepend({"ffffffffffffffff", {"ffffffffffffffff", "已连接", "10.0.0.99"}, {{"id", "ffffffffffffffff"}}});
        table.setRows(snapshot);
        QTest::qWait(50);
        QCOMPARE(table.selectedId(), selectedId);
        QCOMPARE(tree->currentItem(), selected);
        QCOMPARE(tree->columnWidth(0), 235);
        QCOMPARE(tree->header()->visualIndex(3), 2);
        QCOMPARE(tree->horizontalScrollBar()->value(), horizontal);
        QCOMPARE(tree->verticalScrollBar()->value(), vertical);
        QCOMPARE(selected->text(2), QStringLiteral("等待授权"));
        const int count = tree->topLevelItemCount();
        table.setAvailability(false, "服务未响应");
        QCOMPARE(tree->topLevelItemCount(), count);
        QCOMPARE(table.selectedId(), selectedId);
    }

    void renameSurvivesPollingAndRecreation() {
        const auto snapshot = rows(2);
        {
            DataTable table("names", {"名称", "ID", "状态", "IP"});
            table.setRows(snapshot);
            QCOMPARE(table.tree()->topLevelItem(0)->text(0), snapshot[0].id);
            table.tree()->setCurrentItem(table.tree()->topLevelItem(0));
            QTimer::singleShot(80, &table, [] {
                auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                dialog->setTextValue(QStringLiteral("办公室"));
                dialog->accept();
            });
            table.renameSelected();
            table.setRows(snapshot);
            QCOMPARE(table.tree()->topLevelItem(0)->text(0), QStringLiteral("办公室"));
        }
        DataTable reopened("names", {"名称", "ID", "状态", "IP"});
        reopened.setRows(snapshot);
        QCOMPARE(reopened.tree()->topLevelItem(0)->text(0), QStringLiteral("办公室"));
    }

    void detailsKeepAllFieldsAndStayNonModal() {
        DataTable table("details", {"名称", "ID", "状态", "IP"});
        table.setRows(rows(1));
        table.tree()->setCurrentItem(table.tree()->topLevelItem(0));
        table.showDetails();
        auto *dialog = table.findChild<QDialog *>("detailsPopup");
        QVERIFY(dialog);
        QVERIFY(!dialog->isModal());
        auto *json = dialog->findChild<QPlainTextEdit *>("detailsJson");
        QVERIFY(json);
        const auto object = QJsonDocument::fromJson(json->toPlainText().toUtf8()).object();
        QCOMPARE(object["unknown"].toInt(), 42);
        QCOMPARE(object["routes"].toArray()[0].toObject()["target"].toString(), QString("10.0.0.0/24"));
        QCOMPARE(ZeroTier::translate("PLANET"), QStringLiteral("根节点"));
        QCOMPARE(ZeroTier::translate("MOON"), QStringLiteral("中转站"));
        QCOMPARE(ZeroTier::translate("LEAF"), QStringLiteral("成员"));
        QCOMPARE(ZeroTier::translate("DIRECT"), QStringLiteral("直链"));
        QCOMPARE(ZeroTier::translate("RELAY"), QStringLiteral("中转"));
    }

    void mouseDragScrollsBothAxes() {
        DataTable table("drag", {"名称", "ID", "状态", "IP"});
        table.resize(510, 320);
        table.setRows(rows());
        table.show();
        QTest::qWait(50);
        auto *viewport = table.tree()->viewport();
        QTest::mousePress(viewport, Qt::LeftButton, Qt::NoModifier, QPoint(300, 180));
        QTest::mouseMove(viewport, QPoint(180, 70), 50);
        QTest::mouseRelease(viewport, Qt::LeftButton, Qt::NoModifier, QPoint(180, 70));
        QVERIFY(table.tree()->horizontalScrollBar()->value() > 0);
        QVERIFY(table.tree()->verticalScrollBar()->value() > 0);
    }

    void asynchronousPollAndRecovery() {
        QWidget owner;
        owner.show();
        JsonPoller poller(&owner, "listnetworks");
        QSignalSpy updates(&poller, &JsonPoller::updated);
        QSignalSpy availability(&poller, &JsonPoller::availabilityChanged);
        QTRY_VERIFY_WITH_TIMEOUT(updates.size() >= 1, 2000);
        qputenv("ZT_TEST_MODE", "slow");
        poller.refresh();
        bool responsive = false;
        QTimer::singleShot(100, &owner, [&] { responsive = true; });
        QTRY_VERIFY_WITH_TIMEOUT(responsive, 500);
        const int before = updates.size();
        poller.refresh(); // Must not start a second process.
        QTest::qWait(200);
        QCOMPARE(updates.size(), before);
        QTRY_VERIFY_WITH_TIMEOUT(!availability.last()[0].toBool(), 6000);
        qunsetenv("ZT_TEST_MODE");
        QTRY_VERIFY_WITH_TIMEOUT(updates.size() > before, 4000);
        QVERIFY(availability.last()[0].toBool());
        qputenv("ZT_TEST_MODE", "malformed");
        poller.refresh();
        QTRY_VERIFY_WITH_TIMEOUT(!availability.last()[0].toBool(), 2000);
    }

    void uninstallOnlyCleansAfterSuccess() {
#ifdef Q_OS_WIN
        // Both the script and deletion operate only inside this verified test directory.
        QTemporaryDir sandbox(QDir::currentPath() + "/cleanup-XXXXXX");
        QVERIFY(sandbox.isValid());
        QVERIFY(QFileInfo(sandbox.path()).canonicalFilePath().startsWith(QDir::currentPath() + '/'));
        const auto target = QDir(sandbox.path()).filePath("ZeroTier");
        QVERIFY(QDir().mkpath(target + "/One"));
        QFile identity(target + "/One/identity.secret");
        QVERIFY(identity.open(QIODevice::WriteOnly));
        identity.write("test identity");
        identity.close();
        const auto original = qgetenv("ProgramData");
        qputenv("ProgramData", sandbox.path().toLocal8Bit());
        const QString script = ZeroTier::packageScript(true, ZeroTier::executable());
        qputenv("ProgramData", original);
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("ProgramData", sandbox.path());
        environment.insert("ZT_TEST_WINGET_CODE", "23");
        process.setProcessEnvironment(environment);
        const auto encoded = QByteArray(reinterpret_cast<const char *>(script.utf16()), script.size() * 2).toBase64();
        const QStringList args = {"-NoProfile", "-NonInteractive", "-EncodedCommand", QString::fromLatin1(encoded)};
        process.start("powershell.exe", args);
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitCode(), 23);
        QVERIFY(QFile::exists(identity.fileName()));
        environment.insert("ZT_TEST_WINGET_CODE", "0");
        process.setProcessEnvironment(environment);
        process.start("powershell.exe", args);
        QVERIFY(process.waitForFinished(15000));
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(!QFileInfo::exists(target));
#endif
    }

    void powerShellRedirectionDoesNotBlockPayload() {
#ifdef Q_OS_WIN
        // Reproduce the old bug using an actual PowerShell process, without UAC
        // or any system service changes. close() keeps the temporary file locked.
        QTemporaryFile locked;
        QVERIFY(locked.open());
        const QString lockedPath = locked.fileName();
        locked.close();
        const QString brokenScript = "& { Write-Output 'payload'; exit 0 } *> "
                                      + ZeroTier::quotePowerShell(lockedPath);
        const auto brokenEncoded = QByteArray(reinterpret_cast<const char *>(brokenScript.utf16()),
                                              brokenScript.size() * 2).toBase64();
        QProcess broken;
        broken.start("powershell.exe", {"-NoProfile", "-NonInteractive", "-EncodedCommand",
                                        QString::fromLatin1(brokenEncoded)});
        QVERIFY(broken.waitForFinished(10000));
        QCOMPARE(broken.exitCode(), 1);

        // Exercise the same arguments and log lifetime as runElevatedScript.
        QTemporaryDir payloadDirectory;
        QVERIFY(payloadDirectory.isValid());
        const QString actualMarker = payloadDirectory.filePath("payload-ran.txt");
        auto invocation = ZeroTier::preparePowerShell(
            "Set-Content -LiteralPath " + ZeroTier::quotePowerShell(actualMarker)
            + " -Value 'executed'\nWrite-Output '命令实际执行成功'\nexit 0");
        QVERIFY(invocation.directory->isValid());
        QVERIFY(!QFileInfo::exists(invocation.logPath));
        QProcess process;
        process.start("powershell.exe", invocation.arguments);
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(QFileInfo::exists(actualMarker));
        QFile log(invocation.logPath);
        QVERIFY(log.open(QIODevice::ReadOnly));
        QTextStream output(&log);
        QVERIFY(output.readAll().contains(QStringLiteral("命令实际执行成功")));
        log.close();

        auto failure = ZeroTier::preparePowerShell(QStringLiteral("Write-Output '实际失败原因'; exit 23"));
        process.start("powershell.exe", failure.arguments);
        QVERIFY(process.waitForFinished(10000));
        QCOMPARE(process.exitCode(), 23);
        QFile errorLog(failure.logPath);
        QVERIFY(errorLog.open(QIODevice::ReadOnly));
        QTextStream errorOutput(&errorLog);
        QVERIFY(errorOutput.readAll().contains(QStringLiteral("实际失败原因")));
#endif
    }

    void windowLayoutAndVisuals() {
        MainWindow window;
        window.show();
        auto *navigation = window.findChild<QListWidget *>("navigation");
        QVERIFY(navigation);
        auto *splitter = window.findChild<QSplitter *>("mainSplitter");
        QVERIFY(splitter);
        const auto initial = splitter->sizes();
        splitter->setSizes({275, 800});
        QVERIFY(splitter->sizes()[0] > initial[0]);
        navigation->setCurrentRow(3);
        auto *network = window.findChild<NetworkTab *>();
        QVERIFY(network);
        QTRY_COMPARE_WITH_TIMEOUT(network->tree()->topLevelItemCount(), 40, 2500);
        auto *input = window.findChild<QLineEdit *>("networkIdInput");
        input->setText("8056c2");
        auto *selected = network->tree()->topLevelItem(4);
        network->tree()->setCurrentItem(selected);
        const auto id = network->selectedId();
        QTest::qWait(2200);
        QCOMPARE(input->text(), QString("8056c2"));
        QCOMPARE(network->selectedId(), id);
        QCOMPARE(network->tree()->currentItem(), selected);
        for (auto *button : window.findChildren<QPushButton *>()) {
            QVERIFY(!button->text().contains("刷新"));
            QVERIFY(!button->text().contains("Refresh"));
        }
        QVERIFY(QDir().mkpath("screenshots"));
        QVERIFY(window.grab().save("screenshots/networks.png"));
        network->showDetails();
        auto *popup = network->findChild<QDialog *>("detailsPopup");
        QVERIFY(popup);
        QTest::qWait(50);
        QVERIFY(popup->grab().save("screenshots/details.png"));
        popup->close();
        navigation->setCurrentRow(2);
        auto *peers = window.findChild<QTreeWidget *>("peersTable");
        QTRY_COMPARE_WITH_TIMEOUT(peers->topLevelItemCount(), 35, 2500);
        QVERIFY(window.grab().save("screenshots/peers.png"));
        navigation->setCurrentRow(4);
        auto *moons = window.findChild<QTreeWidget *>("moonsTable");
        QTRY_COMPARE_WITH_TIMEOUT(moons->topLevelItemCount(), 1, 2500);
        QCOMPARE(moons->topLevelItem(0)->text(0), QString("0000000123456789"));
        QVERIFY(window.grab().save("screenshots/moons.png"));
        navigation->setCurrentRow(0);
        QVERIFY(window.grab().save("screenshots/service.png"));
        QMessageBox dialog(QMessageBox::Question, "安装 ZeroTier", "使用 winget 下载并安装 ZeroTier，并接受软件包与来源协议，是否继续？",
                          QMessageBox::Yes | QMessageBox::No, &window);
        dialog.setButtonText(QMessageBox::Yes, "继续安装");
        dialog.setButtonText(QMessageBox::No, "取消");
        dialog.show();
        QTest::qWait(50);
        const auto textColor = dialog.button(QMessageBox::Yes)->palette().color(QPalette::ButtonText);
        QVERIFY(textColor.lightness() < 150);
        QVERIFY(dialog.grab().save("screenshots/dialog.png"));
    }
};

int main(int argc, char **argv)
{
#ifdef Q_OS_WIN
    qputenv("QT_QPA_FONTDIR", (qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts").toLocal8Bit());
#endif
    QApplication app(argc, argv);
    GuiTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "GuiTests.moc"
