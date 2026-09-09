#include "dialog_update_available.h"
#include "ui_dialog_update_available.h"

#include <QPushButton>
#include <QApplication>
#include <QPalette>
#include <QRegularExpression>

// Code spans are pulled out before emphasis runs, so markers inside backticks are left
// alone. U+E000 is a private-use codepoint and cannot appear in HTML-escaped input.
static const QChar kCodeSentinel(0xE000);

static QString inlineFormat(const QString &raw) {
    QString s = raw.toHtmlEscaped();

    QStringList codeSpans;
    static QRegularExpression codeRe(R"RX(`([^`]+)`)RX");
    forever {
        auto m = codeRe.match(s);
        if (!m.hasMatch()) break;
        s.replace(m.capturedStart(), m.capturedLength(),
                  QString(kCodeSentinel) + QString::number(codeSpans.size()) + QString(kCodeSentinel));
        codeSpans << m.captured(1);
    }

    // Bold before italic, so ** is not consumed by the single-marker rules. Requiring a
    // non-space at both edges keeps a lone marker (e.g. "*.zip and *.msi") inert.
    s.replace(QRegularExpression(R"RX(\*\*(?=\S)(.+?)(?<=\S)\*\*)RX"), R"RX(<strong>\1</strong>)RX");
    s.replace(QRegularExpression(R"RX((?<!\w)__(?=\S)(.+?)(?<=\S)__(?!\w))RX"), R"RX(<strong>\1</strong>)RX");
    s.replace(QRegularExpression(R"RX(\*(?=\S)([^*]+?)(?<=\S)\*)RX"), R"RX(<em>\1</em>)RX");
    s.replace(QRegularExpression(R"RX((?<![\w_])_(?=\S)([^_]+?)(?<=\S)_(?![\w_]))RX"), R"RX(<em>\1</em>)RX");

    s.replace(QRegularExpression(R"RX(\[([^\]]+)\]\(([^)]+)\))RX"), R"RX(<a href="\2">\1</a>)RX");

    for (int i = 0; i < codeSpans.size(); ++i) {
        s.replace(QString(kCodeSentinel) + QString::number(i) + QString(kCodeSentinel),
                  "<code>" + codeSpans.at(i) + "</code>");
    }
    return s;
}

static QString mdToHtml(const QString &md, const QString &codeBg, const QString &border, const QString &fg) {
    QStringList lines = md.split('\n');
    QString body;
    bool inPre = false;
    bool inQuote = false;
    QString preLines;

    // Open list levels, outermost first. Each entry is "ul" or "ol"; its index is the
    // nesting depth, and indents map onto it so a bullet indented under another nests
    // instead of falling through to the paragraph branch.
    QStringList listStack;
    QList<int> listIndents;

    // Use <p> instead of <h1>/<h2> — Qt forces block margins on heading tags ignoring inline styles
    auto hStyle = [&](int px, bool bottomBorder = true) {
        QString s = QString("style=\"font-size:%1px; font-weight:600; line-height:1.2;"
                            " margin-top:6px; margin-bottom:2px; margin-left:0; margin-right:0;").arg(px);
        if (bottomBorder) s += QString(" padding-bottom:2px; border-bottom:1px solid %1;").arg(border);
        return s + "\"";
    };
    QString pStyle  = "style=\"margin-top:0; margin-bottom:3px; line-height:1.35;\"";
    QString liStyle = "style=\"margin-top:1px; margin-bottom:1px; line-height:1.35;\"";

    auto closeList = [&] {
        while (!listStack.isEmpty()) {
            body += "</" + listStack.takeLast() + ">\n";
            listIndents.removeLast();
        }
    };
    auto closeQuote = [&] {
        if (inQuote) { body += "</blockquote>\n"; inQuote = false; }
    };
    auto closeBlocks = [&] { closeList(); closeQuote(); };

    // Open/close nesting levels until the innermost open list matches this indent.
    auto syncList = [&](const QString &kind, int indent) {
        while (!listStack.isEmpty() && listIndents.last() > indent) {
            body += "</" + listStack.takeLast() + ">\n";
            listIndents.removeLast();
        }
        if (!listStack.isEmpty() && listIndents.last() == indent && listStack.last() != kind) {
            body += "</" + listStack.takeLast() + ">\n";
            listIndents.removeLast();
        }
        if (listStack.isEmpty() || listIndents.last() < indent) {
            body += "<" + kind + " style=\"margin:0 0 4px 0; padding-left:1.6em;\">\n";
            listStack << kind;
            listIndents << indent;
        }
    };

    for (QString raw : lines) {
        // Release bodies arrive over the GitHub API with CRLF; split('\n') leaves the \r.
        if (raw.endsWith('\r')) raw.chop(1);

        if (raw.trimmed().startsWith("```")) {
            if (!inPre) {
                closeBlocks();
                inPre = true;
                preLines.clear();
            } else {
                body += QString(
                    "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\""
                    " style=\"background-color:%1; border:1px solid %2;"
                    " border-radius:6px; margin:2px 0 6px 0;\">"
                    "<tr><td style=\"padding:8px 12px;\">"
                    "<pre style=\"margin:0; font-family:'SFMono-Regular',Consolas,monospace;"
                    " font-size:87%%; line-height:1.4; white-space:pre-wrap; color:%3;\">%4</pre>"
                    "</td></tr></table>\n"
                ).arg(codeBg, border, fg, preLines);
                inPre = false;
            }
            continue;
        }
        if (inPre) { preLines += raw.toHtmlEscaped() + "\n"; continue; }

        const QString trimmed = raw.trimmed();

        if (trimmed.startsWith("#### ")) { closeBlocks(); body += "<p style=\"font-size:13px; font-weight:600; margin-top:4px; margin-bottom:1px; line-height:1.2;\">" + inlineFormat(trimmed.mid(5)) + "</p>\n"; continue; }
        if (trimmed.startsWith("### "))  { closeBlocks(); body += "<p " + hStyle(15, false) + ">" + inlineFormat(trimmed.mid(4)) + "</p>\n"; continue; }
        if (trimmed.startsWith("## "))   { closeBlocks(); body += "<p " + hStyle(17) + ">" + inlineFormat(trimmed.mid(3)) + "</p>\n"; continue; }
        if (trimmed.startsWith("# "))    { closeBlocks(); body += "<p " + hStyle(22) + ">" + inlineFormat(trimmed.mid(2)) + "</p>\n"; continue; }

        static QRegularExpression hrRe(R"RX(^[-*_]{3,}$)RX");
        if (hrRe.match(trimmed).hasMatch()) {
            closeBlocks();
            body += QString("<hr style=\"border:none; border-top:1px solid %1; margin:6px 0;\">\n").arg(border);
            continue;
        }

        // Leading whitespace is allowed and becomes the nesting level.
        static QRegularExpression listRe(R"RX(^([ \t]*)[-*+] (.*)$)RX");
        auto lm = listRe.match(raw);
        if (lm.hasMatch()) {
            closeQuote();
            syncList("ul", lm.captured(1).size());
            body += "<li " + liStyle + ">" + inlineFormat(lm.captured(2)) + "</li>\n";
            continue;
        }

        static QRegularExpression olRe(R"RX(^([ \t]*)\d+\. (.*)$)RX");
        auto om = olRe.match(raw);
        if (om.hasMatch()) {
            closeQuote();
            syncList("ol", om.captured(1).size());
            body += "<li " + liStyle + ">" + inlineFormat(om.captured(2)) + "</li>\n";
            continue;
        }

        static QRegularExpression quoteRe(R"RX(^[ \t]*> ?(.*)$)RX");
        auto qm = quoteRe.match(raw);
        if (qm.hasMatch()) {
            closeList();
            if (!inQuote) { body += "<blockquote>\n"; inQuote = true; }
            body += "<p " + pStyle + ">" + inlineFormat(qm.captured(1)) + "</p>\n";
            continue;
        }

        if (trimmed.isEmpty()) { closeBlocks(); continue; }
        closeBlocks();
        body += "<p " + pStyle + ">" + inlineFormat(trimmed) + "</p>\n";
    }
    closeBlocks();
    return body;
}

static QString buildHtml(const QString &md, bool dark) {
    QString border   = dark ? "#30363d" : "#d0d7de";
    QString codeBg   = dark ? "#161b22" : "#f6f8fa";
    QString inlineBg = dark ? "#2d333b" : "#eef0f3";
    QString link     = dark ? "#58a6ff" : "#0969da";
    QString quoteFg  = dark ? "#8b949e" : "#656d76";
    QString fg       = dark ? "#e6edf3" : "#1f2328";

    QString css = QString(R"(
body { font-family: -apple-system,"Segoe UI",Helvetica,Arial,sans-serif; font-size: 14px; line-height: 1.35; color: %1; margin: 0; padding: 2px 4px 8px 4px; }
h1 { font-size: 1.7em; font-weight: 600; border-bottom: 1px solid %2; padding-bottom: .2em; margin: .6em 0 .2em; }
h2 { font-size: 1.3em; font-weight: 600; border-bottom: 1px solid %2; padding-bottom: .2em; margin: .6em 0 .2em; }
h3 { font-size: 1.1em; font-weight: 600; margin: .5em 0 .15em; }
h4 { font-size: 1em;   font-weight: 600; margin: .4em 0 .1em; }
p  { margin: 0 0 .65em; }
a  { color: %3; text-decoration: none; }
a:hover { text-decoration: underline; }
ul, ol { margin: 0 0 .65em; padding-left: 1.6em; }
li { margin: .15em 0; }
code { font-family: "SFMono-Regular",Consolas,"Liberation Mono",Menlo,monospace; font-size: 85%; background-color: %4; border-radius: 4px; padding: .15em .4em; }
blockquote { border-left: 4px solid %2; color: %5; margin: 0 0 .65em; padding: .3em .8em; }
hr { border: none; border-top: 1px solid %2; margin: 1em 0; }
)").arg(fg, border, link, inlineBg, quoteFg);

    return "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>"
           + css + "</style></head><body>"
           + mdToHtml(md, codeBg, border, fg)
           + "</body></html>";
}

DialogUpdateAvailable::DialogUpdateAvailable(
    const QString &currentVersion,
    const QString &assetName,
    const QString &channel,
    const QString &releaseNote,
    bool allowUpdater,
    QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogUpdateAvailable)
{
    ui->setupUi(this);

    // Fix groupbox height so labels are actually centered — Qt doesn't center layout content in groupboxes
    ui->groupBoxInfo->setFixedHeight(ui->groupBoxInfo->sizeHint().height());

    ui->labelCurrent->setText(currentVersion);
    // Extract version number from asset filename (e.g. "proxor-1.5.4-windows-x64.zip" → "1.5.4")
    static QRegularExpression verRe(R"((\d+\.\d+[\.\d]*))", QRegularExpression::CaseInsensitiveOption);
    auto vm = verRe.match(assetName);
    ui->labelAvailable->setText(vm.hasMatch() ? vm.captured(1) : assetName);
    ui->labelChannel->setText(channel);

    bool dark = QApplication::palette().window().color().lightness() < 128;
    ui->textBrowser->setHtml(
        releaseNote.isEmpty() ? tr("No release notes available.") : buildHtml(releaseNote, dark));

    // Cancel button (already in buttonBox from .ui)
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (allowUpdater) {
        auto *dlBtn = ui->buttonBox->addButton(tr("Download and Restart"), QDialogButtonBox::AcceptRole);
        connect(dlBtn, &QPushButton::clicked, this, [this] { m_action = Download; accept(); });
    }

    auto *pageBtn = ui->buttonBox->addButton(tr("Open Release Page"), QDialogButtonBox::ActionRole);
    connect(pageBtn, &QPushButton::clicked, this, [this] { m_action = OpenPage; accept(); });
}

DialogUpdateAvailable::~DialogUpdateAvailable() {
    delete ui;
}
