#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDebug>
#include <QThread>
#include <QTimer>
#include <QDir>
#include <QCoreApplication>

// 包含项目中使用的模块
#include "view/page_controller.h"
#include "user_session/user_session.h"
#include "dispatcher/message_dispatcher.h"
#include "network/network_manager.h"
#include "base/base_widget.h"
#include"base/theme_manager.h"
#include"word_collect.h"
#include"word_search.h"
#include"word_vocabulary.h"

#include"base/theme_manager.h"
#include"base/connection_overlay.h"

#include"system/system_sidebar.h"
#include"word/word_card.h"
#include"word/interactive_wordcard.h"
#include"search/search_base.h"
#include"search/searchhistory_widget.h"

#include"view/main_page.h"

int main(int argc, char *argv[])
{
    // 设置环境变量帮助调试
    // qputenv("QT_LOGGING_RULES", "*.debug=true");

    qDebug() << "=== 启动 Network Dictionary 应用 ===";
    qDebug() << "应用程序路径:" << QCoreApplication::applicationDirPath();
    qDebug() << "当前工作目录:" << QDir::currentPath();

    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("Network Dictionary");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("NetworkDict");

    qDebug() << "QApplication 创建成功";

    try {
        // 创建主窗口
        BaseWidget *window = new BaseWidget();
        qDebug() << "BaseWidget 创建成功";

        window->resize(1000, 700);
        window->setWindowTitle("Network Dictionary");

        // window->initialize();  // 手动调用初始化
        qDebug() << "BaseWidget 初始化完成";

        QApplication::processEvents();


        // // ========== 主题/QSS 测试代码开始 ==========



        // // 2. 添加一个切换主题按钮（直接放在 window 上，方便测试）
        // QPushButton *themeToggleBtn = new QPushButton("🌙 切换主题", window);
        // themeToggleBtn->setGeometry(window->width() - 140, 20, 120, 36);
        // themeToggleBtn->setObjectName("themeToggleBtn");  // 方便在 QSS 中单独设置样式
        // themeToggleBtn->raise();  // 确保按钮在最上层
        // themeToggleBtn->show();

        // QObject::connect(themeToggleBtn, &QPushButton::clicked, [themeToggleBtn]() {
        //     ThemeManager::instance()->toggleTheme();
        //     UITheme current = ThemeManager::instance()->currentTheme();  // 假设 ThemeManager 有 currentTheme() getter
        //     QString text = (current == UITheme::Light) ? "🌙 切换主题" : "☀️ 切换主题";
        //     themeToggleBtn->setText(text);
        //     qDebug() << "主题已切换为:" << (current == UITheme::Light ? "Light" : "Dark");
        // });

        // // 3. 添加一个状态标签，测试 BaseWidget 的 setUIState 动态属性
        // QLabel *statusLabel = new QLabel("当前状态: Normal", window);
        // statusLabel->setGeometry(20, 20, 200, 30);
        // statusLabel->setObjectName("statusLabel");

        // QPushButton *loadingBtn = new QPushButton("设为 Loading", window);
        // loadingBtn->setGeometry(20, 60, 120, 30);
        // QObject::connect(loadingBtn, &QPushButton::clicked, [window]() {
        //     window->setUIState(UIState::Loading);
        //     qDebug() << "UIState 设置为 Loading";
        // });

        // QPushButton *errorBtn = new QPushButton("设为 Error", window);
        // errorBtn->setGeometry(150, 60, 120, 30);
        // QObject::connect(errorBtn, &QPushButton::clicked, [window]() {
        //     window->setUIState(UIState::Error);
        //     qDebug() << "UIState 设置为 Error";
        // });

        // QPushButton *normalBtn = new QPushButton("设为 Normal", window);
        // normalBtn->setGeometry(280, 60, 120, 30);
        // QObject::connect(normalBtn, &QPushButton::clicked, [window]() {
        //     window->setUIState(UIState::Normal);
        //     qDebug() << "UIState 设置为 Normal";
        // });

        // // 4. 测试消息悬浮层（新增）
        // QPushButton *successMsgBtn = new QPushButton("显示成功消息", window);
        // successMsgBtn->setGeometry(20, 100, 120, 30);
        // QObject::connect(successMsgBtn, &QPushButton::clicked, [window]() {
        //     window->showMessage("操作成功完成！", false, 3000);
        //     qDebug() << "显示成功消息";
        // });

        // QPushButton *errorMsgBtn = new QPushButton("显示错误消息", window);
        // errorMsgBtn->setGeometry(150, 100, 120, 30);
        // QObject::connect(errorMsgBtn, &QPushButton::clicked, [window]() {
        //     window->showMessage("网络连接失败，请检查网络设置", true, 0);
        //     qDebug() << "显示错误消息";
        // });

        // QPushButton *clearMsgBtn = new QPushButton("清除消息", window);
        // clearMsgBtn->setGeometry(280, 100, 120, 30);
        // QObject::connect(clearMsgBtn, &QPushButton::clicked, [window]() {
        //     window->clearMessage();
        //     qDebug() << "手动清除消息";
        // });

        // // ========== 主题/QSS 测试代码结束 ==========





        // 创建 NetworkManager
        NetworkManager *networkManager = new NetworkManager();
        qDebug() << "NetworkManager 创建成功";

        // 创建 MessageDispatcher
        MessageDispatcher *messageDispatcher = new MessageDispatcher();
        qDebug() << "MessageDispatcher 创建成功";

        // 获取 UserSession 实例
        UserSession *userSession = UserSession::instance();
        qDebug() << "UserSession 实例获取成功:" << (userSession != nullptr);



        // 获取各个功能模块实例
        WordSearch *wordSearch = WordSearch::instance();
        WordCollect *wordCollect = WordCollect::instance();
        WordVocabulary *wordVocabulary = WordVocabulary::instance();
        qDebug() << "功能模块实例获取成功";

        // 设置 MessageDispatcher
        messageDispatcher->setNetworkManager(networkManager);
        messageDispatcher->setUserSession(userSession);
        messageDispatcher->setWordSearch(wordSearch);
        messageDispatcher->setWordCollect(wordCollect);
        messageDispatcher->setWordVocabulary(wordVocabulary);
        qDebug() << "MessageDispatcher 配置完成";

        userSession->setMessageDispatcher(messageDispatcher);
        wordSearch->setMessageDispatcher(messageDispatcher);
        wordCollect->setMessageDispatcher(messageDispatcher);
        wordVocabulary->setMessageDispatcher(messageDispatcher);
        qDebug() << "各模块 MessageDispatcher 设置完成";

        // 初始化 UserSession
        userSession->initialize();
        qDebug() << "UserSession 初始化完成";

        // 启动消息分发器
        messageDispatcher->start();
        qDebug() << "MessageDispatcher 启动完成";

        // 创建 PageController
        PageController *pageController = new PageController(window);

        // //创建MainPage
        // MainPage *mainPage=new MainPage(window);


        // 添加到主窗口
        window->mainLayout()->addWidget(pageController);
        qDebug() << "PageController 添加到窗口完成";

        // 创建连接状态遮罩：未连接 / 断线重连时阻塞输入并显示加载画面
        ConnectionOverlay* connectionOverlay = new ConnectionOverlay(window);
        connectionOverlay->setBlockTarget(pageController);
        connectionOverlay->setGiveUpAttempts(10);
        QObject::connect(networkManager, &NetworkManager::connectionEstablished,
                         connectionOverlay, &ConnectionOverlay::hideOverlay);
        QObject::connect(networkManager, &NetworkManager::connectionLost,
                         connectionOverlay, &ConnectionOverlay::showReconnecting);
        QObject::connect(networkManager, &NetworkManager::reconnectAttempt,
                         connectionOverlay, &ConnectionOverlay::updateAttempt);
        QObject::connect(connectionOverlay, &ConnectionOverlay::retryRequested,
                         networkManager, [networkManager]() {
                             networkManager->connectToServer();
                         });

        // window->mainLayout()->addWidget(mainPage);
        // qDebug()<<"MainPage 添加到窗口成功";

        qDebug() << "模块初始化完成";

        // 连接服务器
        if (networkManager->isConnected()) {
            qDebug() << "已经连接到服务器";
        } else {
            qDebug() << "正在连接到服务器...";
            networkManager->connectToServer();
            // 若 600ms 内仍未连上（连接成功会先触发 hideOverlay），再弹出遮罩，
            // 避免服务器在线时加载画面一闪而过。
            connectionOverlay->armPendingShow(600);
            // 等待建立连接 - 使用QTimer避免阻塞
            QTimer::singleShot(1000, [&]() {
                qDebug() << "连接等待完成";
            });
            QApplication::processEvents();
        }

        // // ========== SystemSidebar 测试代码开始 ==========

        // // 创建侧边栏（父窗口是主窗口）
        // SystemSidebar *systemSidebar = new SystemSidebar(window, userSession);
        // systemSidebar->setObjectName("systemSidebar");

        // // 创建主内容区域（用于和侧边栏并排）
        // QWidget *contentArea = new QWidget(window);
        // //contentArea->setAttribute(Qt::WA_StyledBackground, true);
        // contentArea->setObjectName("contentArea");

        // // 添加一个标签显示当前页面
        // QVBoxLayout *contentLayout = new QVBoxLayout(contentArea);
        // QLabel *pageLabel = new QLabel("主内容区域\n(这里显示各个功能页面)", contentArea);
        // pageLabel->setAlignment(Qt::AlignCenter);
        // contentLayout->addWidget(pageLabel);


        // // ========== SearchHistoryWidget 集成测试开始 ==========

        // // 1. 创建带历史记录的搜索框
        // SearchHistoryWidget *searchHistory = new SearchHistoryWidget(contentArea, userSession);
        // searchHistory->setObjectName("mainSearchHistory");
        // searchHistory->setPlaceholder(QStringLiteral("输入单词并搜索，例如: hello / perseverance ..."));
        // searchHistory->setMaximumWidth(600);
        // contentLayout->addWidget(searchHistory, 0, Qt::AlignHCenter);

        // // 2. 搜索结果展示区
        // QLabel *searchTipLabel = new QLabel(
        //     QStringLiteral("🔍 在上方输入单词并搜索，单词卡片将显示在这里"),
        //     contentArea
        //     );
        // searchTipLabel->setObjectName("searchTipLabel");
        // searchTipLabel->setAlignment(Qt::AlignCenter);
        // searchTipLabel->setStyleSheet("color: #888888; font-size: 14px; padding: 20px;");
        // contentLayout->addWidget(searchTipLabel, 0, Qt::AlignHCenter);

        // QWidget *searchResultsWidget = new QWidget(contentArea);
        // searchResultsWidget->setObjectName("searchResultsWidget");
        // QVBoxLayout *resultsLayout = new QVBoxLayout(searchResultsWidget);
        // resultsLayout->setContentsMargins(0, 10, 0, 0);
        // resultsLayout->setSpacing(12);
        // resultsLayout->addStretch(1);
        // contentLayout->addWidget(searchResultsWidget, 0, Qt::AlignHCenter);

        // // 3. 搜索槽：创建单词卡片
        // auto doSearch = [=](const QString &keyword) {
        //     if (keyword.isEmpty()) return;

        //     qDebug() << "[SearchHistoryWidget Test] 搜索请求:" << keyword;
        //     searchTipLabel->setVisible(false);

        //     WordData result;
        //     result.word = keyword;

        //     if (keyword.compare(QStringLiteral("perseverance"), Qt::CaseInsensitive) == 0) {
        //         result.phonetic    = QStringLiteral("ˌpɜːsɪˈvɪərəns");
        //         result.meaning     = QStringLiteral("n. 毅力；坚持不懈");
        //         result.example     = QStringLiteral("Success requires perseverance and hard work.");
        //         result.translation = QStringLiteral("成功需要毅力和努力工作。");
        //     }
        //     else if (keyword.compare(QStringLiteral("hello"), Qt::CaseInsensitive) == 0) {
        //         result.phonetic    = QStringLiteral("həˈləʊ");
        //         result.meaning     = QStringLiteral("int. 你好；喂\nn. 问候");
        //         result.example     = QStringLiteral("Hello, how are you today?");
        //         result.translation = QStringLiteral("你好，你今天好吗？");
        //     }
        //     else {
        //         result.phonetic    = QStringLiteral("/sɜːrtʃ/");
        //         result.meaning     = QStringLiteral("n. 查询结果\nv. 检索");
        //         result.example     = QStringLiteral("The word \"%1\" was searched in the dictionary.").arg(keyword);
        //         result.translation = QStringLiteral("已在词典中检索到单词 \"%1\"。").arg(keyword);
        //     }
        //     result.isCollected  = false;
        //     result.isVocabulary = false;

        //     InteractiveWordCard *card = new InteractiveWordCard(
        //         result, wordCollect, wordVocabulary, searchResultsWidget
        //         );
        //     card->setObjectName(QStringLiteral("searchResultCard_%1").arg(keyword));
        //     card->setMaximumWidth(600);
        //     card->setMinimumWidth(400);

        //     resultsLayout->insertWidget(resultsLayout->count() - 1, card, 0,
        //                                 Qt::AlignHCenter | Qt::AlignTop);

        //     // 限制最多保留 3 个搜索结果
        //     while (resultsLayout->count() > 4) {
        //         QLayoutItem *item = resultsLayout->takeAt(0);
        //         if (item && item->widget()) {
        //             item->widget()->deleteLater();
        //         }
        //         delete item;
        //     }
        // };

        // // 连接搜索信号
        // QObject::connect(searchHistory, &SearchHistoryWidget::searchRequested, searchHistory, doSearch);

        // // 4. 清除槽：清空搜索结果
        // QObject::connect(searchHistory, &SearchHistoryWidget::clearClicked, [=]() {
        //     while (resultsLayout->count() > 1) {
        //         QLayoutItem *item = resultsLayout->takeAt(0);
        //         if (item && item->widget()) {
        //             item->widget()->deleteLater();
        //         }
        //         delete item;
        //     }
        //     searchTipLabel->setVisible(true);
        //     qDebug() << "[SearchHistoryWidget Test] 搜索结果已清空";
        // });

        // // 【新增】5. 输入框被删空时（Backspace / Delete），自动清空卡片
        // QObject::connect(searchHistory, &SearchHistoryWidget::keywordChanged, [=](const QString &text) {
        //     if (text.isEmpty()) {
        //         while (resultsLayout->count() > 1) {
        //             QLayoutItem *item = resultsLayout->takeAt(0);
        //             if (item && item->widget()) {
        //                 item->widget()->deleteLater();
        //             }
        //             delete item;
        //         }
        //         searchTipLabel->setVisible(true);
        //         qDebug() << "[SearchHistoryWidget Test] 输入框已清空，搜索结果已清空";
        //     }
        // });

        // // 5. 历史记录功能测试 - 预填充测试数据
        // // 模拟用户已登录状态，添加一些历史记录
        // searchHistory->addHistory(QStringLiteral("hello"));
        // searchHistory->addHistory(QStringLiteral("world"));
        // searchHistory->addHistory(QStringLiteral("perseverance"));
        // searchHistory->addHistory(QStringLiteral("dictionary"));
        // searchHistory->addHistory(QStringLiteral("network"));
        // qDebug() << "[SearchHistoryWidget Test] 预填充历史记录完成，当前数量:" << searchHistory->historyCount();

        // // 6. 连接历史项点击信号 - 点击历史项自动搜索
        // QObject::connect(searchHistory, &SearchHistoryWidget::historyItemClicked,
        //                  [=](const QString &keyword) {
        //                      qDebug() << "[SearchHistoryWidget Test] 历史项点击:" << keyword;
        //                  });

        // // 7. 连接历史清除信号
        // QObject::connect(searchHistory, &SearchHistoryWidget::historyCleared, [=]() {
        //     qDebug() << "[SearchHistoryWidget Test] 历史记录已清除";
        // });

        // // 8. 测试配置接口
        // qDebug() << "[SearchHistoryWidget Test] 最大历史数量:" << searchHistory->maxHistoryCount();
        // searchHistory->setMaxHistoryCount(10);
        // qDebug() << "[SearchHistoryWidget Test] 修改后最大历史数量:" << searchHistory->maxHistoryCount();

        // // 9. 测试获取历史列表
        // QStringList historyList = searchHistory->getHistory();
        // qDebug() << "[SearchHistoryWidget Test] 当前历史列表:" << historyList;

        // // 10. 测试访问器
        // qDebug() << "[SearchHistoryWidget Test] 历史列表控件:" << searchHistory->getHistoryList();
        // qDebug() << "[SearchHistoryWidget Test] 历史容器控件:" << searchHistory->getHistoryContainer();

        // // ========== SearchHistoryWidget 集成测试结束 ==========

        // // 创建水平布局：侧边栏 + 内容区
        // QHBoxLayout *mainHBox = new QHBoxLayout(window);
        // mainHBox->setContentsMargins(0, 0, 0, 0);
        // mainHBox->setSpacing(0);
        // mainHBox->addWidget(systemSidebar, 0, Qt::AlignLeft);
        // mainHBox->addWidget(contentArea, 1);  // contentArea 占据剩余空间

        // // 把水平布局设置到窗口（替换 BaseWidget 原来的垂直布局）
        // // 注意：BaseWidget 构造函数已经创建了 m_mainLayout，需要先清理
        // QLayout *oldLayout = window->layout();
        // if (oldLayout) {
        //     // 把旧布局里的控件移到新布局
        //     while (QLayoutItem *item = oldLayout->takeAt(0)) {
        //         if (QWidget *w = item->widget()) {
        //             mainHBox->addWidget(w);
        //         }
        //         delete item;
        //     }
        //     delete oldLayout;
        // }
        // window->setLayout(mainHBox);

        // // 连接侧边栏信号，测试页面切换
        // QObject::connect(systemSidebar, &SystemSidebar::showCollectPageRequested, [pageLabel]() {
        //     pageLabel->setText("📚 收藏页面\n(显示用户收藏的单词)");
        //     qDebug() << "切换到收藏页面";
        // });

        // QObject::connect(systemSidebar, &SystemSidebar::showVocabularyPageRequested, [pageLabel]() {
        //     pageLabel->setText("⭐ 生词本页面\n(显示用户的生词本)");
        //     qDebug() << "切换到生词本页面";
        // });

        // QObject::connect(systemSidebar, &SystemSidebar::themeToggleRequested, [pageLabel]() {
        //     pageLabel->setText("🎨 主题已切换\n(观察整体颜色变化)");
        //     qDebug() << "主题切换信号触发";
        // });

        // QObject::connect(systemSidebar, &SystemSidebar::logoutRequested, [pageLabel, userSession]() {
        //     pageLabel->setText("🚪 退出登录\n(用户会话已清除)");
        //     qDebug() << "退出登录信号触发";
        // });

        // // 测试展开/收起信号
        // QObject::connect(systemSidebar, &SystemSidebar::expandedChanged, [](bool expanded) {
        //     qDebug() << "侧边栏状态变化：" << (expanded ? "展开" : "收起");
        // });

        // qDebug() << "SystemSidebar 创建并布局完成";

        // // ========== SystemSidebar 测试代码结束 ==========

        // // ========== InteractiveWordCard 测试代码开始 ==========

        // // // 创建测试单词数据
        // // WordData testWord;
        // // testWord.word = "ambitious";
        // // testWord.phonetic = "æmˈbɪʃəs";
        // // testWord.meaning = "adj. 有雄心的；有抱负的 n. 野心勃勃的人";
        // // testWord.example = "She is an ambitious student who wants to become a doctor.";
        // // testWord.translation = "她是一个有抱负的学生，想成为一名医生。";
        // // testWord.isCollected=true;
        // // testWord.isVocabulary=true;

        // // // 创建交互式单词卡片（传入 WordCollect 和 WordVocabulary 实例）
        // // InteractiveWordCard *interactiveCard = new InteractiveWordCard(
        // //     testWord,
        // //     wordCollect,      // 从 main.cpp 获取的 WordCollect 实例
        // //     wordVocabulary,   // 从 main.cpp 获取的 WordVocabulary 实例
        // //     contentArea
        // //     );
        // // interactiveCard->setObjectName("testInteractiveCard");

        // // // 设置卡片固定宽度，高度自适应
        // // interactiveCard->setMaximumWidth(800);
        // // interactiveCard->setMinimumWidth(400);

        // // // 添加到内容区域布局（放在 pageLabel 下方）
        // // contentLayout->addWidget(interactiveCard, 0, Qt::AlignHCenter | Qt::AlignTop);

        // // 添加说明文字
        // // QLabel *hintLabel = new QLabel("💡 双击卡片展开/折叠例句\n❤️ 点击星形收藏\n🔊 点击喇叭发音\n⭐ 点击书签加入生词本", contentArea);
        // // hintLabel->setAlignment(Qt::AlignCenter);
        // // hintLabel->setObjectName("hintLabel");
        // // contentLayout->addWidget(hintLabel);

        // // // 连接卡片信号
        // // QObject::connect(interactiveCard, &InteractiveWordCard::clicked, []() {
        // //     qDebug() << "InteractiveWordCard 被点击";
        // // });

        // // QObject::connect(interactiveCard, &InteractiveWordCard::doubleClicked, [interactiveCard]() {
        // //     qDebug() << "InteractiveWordCard 被双击，当前展开状态：" << interactiveCard->isExpanded();
        // // });

        // // QObject::connect(interactiveCard, &InteractiveWordCard::favoriteToggled,
        // //                  [](const QString& word, bool favorite) {
        // //                      qDebug() << "收藏状态变化：" << word << (favorite ? "已收藏" : "取消收藏");
        // //                  });

        // // QObject::connect(interactiveCard, &InteractiveWordCard::pronunciationRequested,
        // //                  [](const QString& word) {
        // //                      qDebug() << "发音请求：" << word;
        // //                  });

        // // QObject::connect(interactiveCard, &InteractiveWordCard::addToVocabularyRequested,
        // //                  [](const WordData& data) {
        // //                      qDebug() << "添加到生词本：" << data.word;
        // //                  });

        // // // 添加第二个交互式单词卡片（用于测试列表效果）
        // // WordData testWord2;
        // // testWord2.word = "perseverance";
        // // testWord2.phonetic = "ˌpɜːsɪˈvɪərəns";
        // // testWord2.meaning = "n. 毅力；坚持不懈";
        // // testWord2.example = "Success requires perseverance and hard work.";
        // // testWord2.translation = "成功需要毅力和努力工作。";
        // // testWord2.isCollected=true;
        // // testWord2.isVocabulary=true;

        // // InteractiveWordCard *interactiveCard2 = new InteractiveWordCard(
        // //     testWord2,
        // //     wordCollect,
        // //     wordVocabulary,
        // //     contentArea
        // //     );
        // // interactiveCard2->setObjectName("testInteractiveCard2");
        // // interactiveCard2->setContentsMargins(0,0,0,0);
        // // interactiveCard2->setMaximumWidth(600);
        // // interactiveCard2->setMinimumWidth(400);
        // // contentLayout->addWidget(interactiveCard2, 0, Qt::AlignHCenter | Qt::AlignTop);

        // // // 在卡片之间添加弹性空间，使卡片靠上对齐
        // // contentLayout->addStretch(1);

        // // qDebug() << "InteractiveWordCard 测试卡片创建完成";

        // // ========== InteractiveWordCard 测试代码结束 ==========



        // 显示主窗口
        window->show();
        qDebug() << "主窗口显示成功";

        qDebug() << "应用启动完成，进入事件循环";

        int result = app.exec();

        qDebug() << "应用即将退出，退出码:" << result;

        // // 清理资源
        // if (pageController) {
        //     pageController->hide();
        //     delete pageController;
        //     qDebug() << "PageController 已清理";
        // }

        if (userSession) {
            userSession->setMessageDispatcher(nullptr);
            //userSession->reset();
            qDebug() << "UserSession 已清理";
        }

        if (messageDispatcher) {
            messageDispatcher->stop();
            messageDispatcher->deleteLater();
            qDebug() << "MessageDispatcher 已清理";
        }

        if (networkManager && networkManager->isConnected()) {
            networkManager->disconnectFromServer();
            networkManager->deleteLater();
            qDebug() << "NetworkManager 已清理";
        }

        if (window) {
            window->hide();
            delete window;
            qDebug() << "主窗口已清理";
        }

        qDebug() << "资源清理完成";

        return result;

    } catch (const std::exception& e) {
        qCritical() << "捕获到异常:" << e.what();
        return -1;
    } catch (...) {
        qCritical() << "捕获到未知异常";
        return -1;
    }
}
