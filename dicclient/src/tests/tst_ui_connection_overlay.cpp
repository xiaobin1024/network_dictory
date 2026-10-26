// tst_ui_connection_overlay.cpp
/*
 * ConnectionOverlay 组件单元测试（GoogleTest）
 *
 * 覆盖断网遮罩的核心行为（确定性，不依赖真实服务器）：
 *   - 初始隐藏，且不会阻塞底层页面；
 *   - showConnecting（初始连接态）后可见，并禁用 block target（阻塞输入）；
 *   - showReconnecting（重连态）后可见，但【不阻塞】底层页面且遮罩鼠标穿透，
 *     以便离线时搜索框仍可查询本地缓存（LocalWordCache）；
 *   - updateAttempt 正确累加“已尝试 N 次”，阻塞态达到阈值切换为“提示重启”文案；
 *   - hideOverlay 后隐藏并恢复底层页面输入；
 *   - armPendingShow 在延迟内若未 hide 则弹出遮罩（避免秒连闪烁）。
 *
 * QWidget 测试必须先创建 QApplication。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QSignalSpy>
#include <QWidget>
#include <QTimer>
#include <QEventLoop>

#include "ui/base/connection_overlay.h"

using namespace testing;

class QtOverlayTestEnvironment : public ::testing::Environment
{
public:
    void SetUp() override
    {
        static int argc = 1;
        static char arg0[] = "tst_ui_connection_overlay";
        static char *argv[] = {arg0, nullptr};
        app = new QApplication(argc, argv);
    }
    void TearDown() override
    {
        delete app;
        app = nullptr;
    }
private:
    QApplication *app = nullptr;
};

::testing::Environment *const qtOverlayEnv =
    ::testing::AddGlobalTestEnvironment(new QtOverlayTestEnvironment);

// 用一个简单的容器窗口 + 子控件模拟被遮挡的页面（如 PageController）
class OverlayHarness : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_window = new QWidget();
        m_window->resize(800, 600);
        m_blockTarget = new QWidget(m_window);  // 模拟被禁用的页面
        m_overlay = new ConnectionOverlay(m_window);
        m_overlay->setBlockTarget(m_blockTarget);
        m_overlay->setGiveUpAttempts(3);
        m_window->show();
    }
    void TearDown() override
    {
        delete m_window;  // 会递归删除 m_blockTarget 与 m_overlay
        m_window = nullptr;
    }

    QWidget*          m_window{nullptr};
    QWidget*          m_blockTarget{nullptr};
    ConnectionOverlay* m_overlay{nullptr};
};

TEST_F(OverlayHarness, InitiallyHiddenAndUnblocked)
{
    EXPECT_FALSE(m_overlay->isVisible());
    EXPECT_TRUE(m_blockTarget->isEnabled());
}

TEST_F(OverlayHarness, ShowConnectingBlocksInput)
{
    m_overlay->showConnecting();
    EXPECT_TRUE(m_overlay->isVisible());
    EXPECT_FALSE(m_blockTarget->isEnabled());   // 底层输入被阻塞
}

TEST_F(OverlayHarness, ShowReconnectingKeepsInputUnblocked)
{
    // 重连态必须“非阻塞”：底层页面（含搜索框）保持可交互，离线缓存查询才可用。
    m_overlay->showReconnecting();
    EXPECT_TRUE(m_overlay->isVisible());
    EXPECT_TRUE(m_blockTarget->isEnabled());   // 输入不被阻塞
    // 遮罩鼠标穿透，点击落到底层页面而非遮罩
    EXPECT_TRUE(m_overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
}

TEST_F(OverlayHarness, ShowConnectingIsBlocking)
{
    // 初始连接态仍保持阻塞，避免未连上时空操作。
    m_overlay->showConnecting();
    EXPECT_TRUE(m_overlay->isVisible());
    EXPECT_FALSE(m_blockTarget->isEnabled());
    EXPECT_FALSE(m_overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
}

TEST_F(OverlayHarness, ReconnectOverlayStaysUnblockedAtThreshold)
{
    // 重连态即便达到重连阈值，也不应弹出“立即重试”死按钮（自动重连接管）。
    m_overlay->showReconnecting();
    m_overlay->updateAttempt(3, 100);   // 达到阈值
    QPushButton* retry = m_overlay->findChild<QPushButton*>("connRetryBtn");
    ASSERT_NE(retry, nullptr);
    EXPECT_FALSE(retry->isVisible());   // 重连态不显示手动重试
    EXPECT_TRUE(m_blockTarget->isEnabled());
}

TEST_F(OverlayHarness, UpdateAttemptCountsAndThresholdShowsHint)
{
    m_overlay->showConnecting();
    m_overlay->updateAttempt(1, 100);
    m_overlay->updateAttempt(2, 100);
    // 尚未达阈值，不应出现重启提示（按钮隐藏）
    EXPECT_FALSE(m_overlay->findChild<QPushButton*>("connRetryBtn")->isVisible());

    m_overlay->updateAttempt(3, 100);  // 达到阈值
    QPushButton* retry = m_overlay->findChild<QPushButton*>("connRetryBtn");
    QLabel* hint = m_overlay->findChild<QLabel*>("connHintLabel");
    ASSERT_NE(retry, nullptr);
    ASSERT_NE(hint, nullptr);
    EXPECT_TRUE(retry->isVisible());    // “立即重试”按钮出现
    EXPECT_TRUE(hint->isVisible());     // 重启提示文案出现
    EXPECT_FALSE(hint->text().isEmpty());
}

TEST_F(OverlayHarness, HideOverlayRestoresInput)
{
    m_overlay->showConnecting();
    EXPECT_FALSE(m_blockTarget->isEnabled());
    m_overlay->hideOverlay();
    EXPECT_FALSE(m_overlay->isVisible());
    EXPECT_TRUE(m_blockTarget->isEnabled());   // 输入恢复
}

TEST_F(OverlayHarness, PendingShowPopsOverlayAfterDelay)
{
    // 未连接也未主动 show：延迟后应弹出遮罩
    m_overlay->armPendingShow(50);
    // 用本地事件循环跑过延迟，保证计时器触发
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_TRUE(m_overlay->isVisible());
}

TEST_F(OverlayHarness, PendingShowCancelledByHide)
{
    m_overlay->armPendingShow(50);
    // 连接成功，及时隐藏 -> 延迟后不应弹出
    m_overlay->hideOverlay();
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_FALSE(m_overlay->isVisible());
    EXPECT_TRUE(m_blockTarget->isEnabled());
}
