#include "ZeroTierTab.h"
#include "ServiceTab.h"
#include "InfoTab.h"
#include "PeersTab.h"
#include "NetworkTab.h"
#include "MoonTab.h"
#include "../MainWindow.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QGroupBox>

ZeroTierTab::ZeroTierTab(MainWindow *mainWindow, QWidget *parent)
    : QWidget(parent), m_main(mainWindow)
{
    setupUi();
}

void ZeroTierTab::setupUi()
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    m_scrollArea = new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget;
    auto *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setSpacing(12);
    contentLayout->setContentsMargins(6, 6, 6, 6);

    // --- 1. 服务管理 ---
    auto *serviceGroup = new QGroupBox("服务管理");
    auto *serviceLayout = new QVBoxLayout(serviceGroup);
    m_serviceTab = new ServiceTab(m_main, serviceGroup);
    serviceLayout->addWidget(m_serviceTab);
    contentLayout->addWidget(serviceGroup);

    // --- 2. 节点信息 ---
    auto *infoGroup = new QGroupBox("节点信息");
    auto *infoLayout = new QVBoxLayout(infoGroup);
    m_infoTab = new InfoTab(m_main, infoGroup);
    infoLayout->addWidget(m_infoTab);
    contentLayout->addWidget(infoGroup);

    // --- 3. Peers 列表 ---
    auto *peersGroup = new QGroupBox("Peers 列表");
    auto *peersLayout = new QVBoxLayout(peersGroup);
    m_peersTab = new PeersTab(m_main, peersGroup);
    peersLayout->addWidget(m_peersTab);
    contentLayout->addWidget(peersGroup);

    // --- 4. 网络管理 ---
    auto *networkGroup = new QGroupBox("网络管理");
    auto *networkLayout = new QVBoxLayout(networkGroup);
    m_networkTab = new NetworkTab(m_main, networkGroup);
    networkLayout->addWidget(m_networkTab);
    contentLayout->addWidget(networkGroup);

    // --- 5. Moon 管理 ---
    auto *moonGroup = new QGroupBox("Moon 管理");
    auto *moonLayout = new QVBoxLayout(moonGroup);
    m_moonTab = new MoonTab(m_main, moonGroup);
    moonLayout->addWidget(m_moonTab);
    contentLayout->addWidget(moonGroup);

    contentLayout->addStretch();

    m_scrollArea->setWidget(scrollContent);
    outerLayout->addWidget(m_scrollArea);
}

void ZeroTierTab::refreshAll()
{
    // 调用各子 Tab 的公共 refresh 方法更新数据
    m_infoTab->refresh();
    m_peersTab->refresh();
    m_networkTab->refresh();
    // ServiceTab 和 MoonTab 没有独立的 public refresh 方法，
    // 它们的 UI 更新由用户按钮操作触发，此处不强制刷新
}
