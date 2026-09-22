// qt-wasm-demo — a minimal Qt Widgets app used to exercise the
// emscripten-forge Qt6/WebAssembly toolchain.
//
// Deliberately data-science flavoured: a filterable table of fake
// measurements, a text field, a button and a live summary line. Enough
// widgets to prove that layout, fonts, input, model/view and signals all
// survive the trip through wasm32 + JSPI.

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {

const QStringList kSpecies = {"setosa", "versicolor", "virginica"};

void fillModel(QStandardItemModel *model)
{
    model->removeRows(0, model->rowCount());
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 60; ++i) {
        const int cls = rng->bounded(kSpecies.size());
        const double sepal = 4.5 + cls * 0.9 + rng->generateDouble() * 1.2;
        const double petal = 1.2 + cls * 1.8 + rng->generateDouble() * 0.9;
        const double score = 1.0 / (1.0 + std::exp(-(petal - 3.0)));

        QList<QStandardItem *> row;
        row << new QStandardItem(QString("obs-%1").arg(i, 3, 10, QChar('0')));
        row << new QStandardItem(kSpecies.at(cls));
        auto *a = new QStandardItem;
        a->setData(QString::number(sepal, 'f', 2), Qt::DisplayRole);
        auto *b = new QStandardItem;
        b->setData(QString::number(petal, 'f', 2), Qt::DisplayRole);
        auto *c = new QStandardItem;
        c->setData(QString::number(score, 'f', 3), Qt::DisplayRole);
        row << a << b << c;
        model->appendRow(row);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("qt-wasm-demo");

    QWidget window;
    window.setWindowTitle("Qt for WebAssembly — emscripten-forge demo");

    auto *model = new QStandardItemModel(&window);
    model->setHorizontalHeaderLabels({"id", "species", "sepal_len", "petal_len", "p(virginica)"});
    fillModel(model);

    auto *proxy = new QSortFilterProxyModel(&window);
    proxy->setSourceModel(model);
    proxy->setFilterKeyColumn(-1); // match against every column
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    auto *table = new QTableView;
    table->setModel(proxy);
    table->setSortingEnabled(true);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);

    auto *filter = new QLineEdit;
    filter->setPlaceholderText("filter rows (try: virginica)");
    filter->setClearButtonEnabled(true);

    auto *regenerate = new QPushButton("Resample");
    auto *status = new QLabel;
    status->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto refreshStatus = [status, proxy, model] {
        status->setText(QString("%1 / %2 rows shown · Qt %3 · %4")
                            .arg(proxy->rowCount())
                            .arg(model->rowCount())
                            .arg(QT_VERSION_STR)
#ifdef __EMSCRIPTEN__
                            .arg("wasm32 (emscripten-forge)")
#else
                            .arg("native")
#endif
        );
    };

    QObject::connect(filter, &QLineEdit::textChanged, proxy,
                     [proxy, refreshStatus](const QString &text) {
                         proxy->setFilterFixedString(text);
                         refreshStatus();
                     });
    QObject::connect(regenerate, &QPushButton::clicked, &window,
                     [model, refreshStatus] {
                         fillModel(model);
                         refreshStatus();
                     });

    auto *controls = new QHBoxLayout;
    controls->addWidget(filter, 1);
    controls->addWidget(regenerate);

    auto *layout = new QVBoxLayout(&window);
    auto *title = new QLabel("<b>Qt Widgets running in the browser</b>");
    layout->addWidget(title);
    layout->addLayout(controls);
    layout->addWidget(table, 1);
    layout->addWidget(status);

    refreshStatus();
    window.resize(720, 520);
    window.show();

    return app.exec();
}
