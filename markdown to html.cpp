// md2html.cpp
// Simple Markdown -> HTML converter with an optional “type n to download” prompt.
// Coverage: # Headings, **bold**, *italic*, `code`, ``` code blocks ```, links, images,
// lists (-, *, 1.), blockquotes (>), horizontal rules (---, ***), and paragraphs.
// Note: This is a lightweight, line-oriented parser meant for common Markdown; not a full spec.

#include <bits/stdc++.h>
using namespace std;

static string htmlEscape(const string &s) {
    string out; out.reserve(s.size()*1.1);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;";  break;
            case '>': out += "&gt;";  break;
            case '"': out += "&quot;";break;
            case '\'':out += "&#39;"; break;
            default:  out += c;
        }
    }
    return out;
}

static string regexReplaceAll(const string &input, const regex &re, const function<string(const smatch&)> &repl) {
    string result;
    auto begin = input.cbegin();
    auto end = input.cend();
    smatch m;
    while (regex_search(begin, end, m, re)) {
        result.append(begin, m.prefix().second);
        result += repl(m);
        begin = m.suffix().first;
    }
    result.append(begin, end);
    return result;
}

static string processInline(string s) {
    // escape first; then selectively unescape inside HTML tags we produce
    s = htmlEscape(s);

    // images: ![alt](url)
    s = regexReplaceAll(s, regex(R"(!

\[(.*?)\]

\((.*?)\))"), [](const smatch &m){
        string alt = m[1].str(); string url = m[2].str();
        return "<img alt=\"" + alt + "\" src=\"" + url + "\">";
    });

    // links: [text](url)
    s = regexReplaceAll(s, regex(R"(

\[(.*?)\]

\((.*?)\))"), [](const smatch &m){
        string text = m[1].str(); string url = m[2].str();
        return "<a href=\"" + url + "\">" + text + "</a>";
    });

    // bold: **text** or __text__
    s = regexReplaceAll(s, regex(R"(\*\*(.+?)\*\*)"), [](const smatch &m){
        return "<strong>" + m[1].str() + "</strong>";
    });
    s = regexReplaceAll(s, regex(R"(__(.+?)__)"), [](const smatch &m){
        return "<strong>" + m[1].str() + "</strong>";
    });

    // italic: *text* or _text_
    s = regexReplaceAll(s, regex(R"(\*(.+?)\*))"), [](const smatch &m){
        return "<em>" + m[1].str() + "</em>";
    });
    s = regexReplaceAll(s, regex(R"(_(.+?)_)"), [](const smatch &m){
        return "<em>" + m[1].str() + "</em>";
    });

    // inline code: `code`
    s = regexReplaceAll(s, regex(R"(`([^`]+)`)"), [](const smatch &m){
        return "<code>" + m[1].str() + "</code>";
    });

    return s;
}

struct Line {
    string raw;
};

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string inputPath;
    string outputPath;
    // Parse args: md2html [input.md] [-o output.html]
    for (int i=1; i<argc; ++i) {
        string a = argv[i];
        if (a == "-o" && i+1 < argc) {
            outputPath = argv[++i];
        } else if (inputPath.empty()) {
            inputPath = a;
        } else {
            cerr << "Unknown argument: " << a << "\n";
            return 1;
        }
    }

    vector<Line> lines;
    if (!inputPath.empty()) {
        ifstream in(inputPath);
        if (!in) {
            cerr << "Cannot open input file: " << inputPath << "\n";
            return 1;
        }
        string s;
        while (getline(in, s)) lines.push_back({s});
    } else {
        // read stdin
        string s;
        while (getline(cin, s)) lines.push_back({s});
    }

    // State
    bool inCodeBlock = false;
    string codeLang;
    bool inUl = false;
    bool inOl = false;
    bool inBlockquote = false;

    // For paragraphs: accumulate non-empty lines until blank
    vector<string> html;
    auto closeLists = [&](){
        if (inUl) { html.push_back("</ul>"); inUl = false; }
        if (inOl) { html.push_back("</ol>"); inOl = false; }
    };
    auto closeBlockquote = [&](){
        if (inBlockquote) { html.push_back("</blockquote>"); inBlockquote = false; }
    };

    for (size_t i=0; i<lines.size(); ++i) {
        string line = lines[i].raw;

        // handle fenced code blocks
        if (!inCodeBlock) {
            // opening fence ```lang?
            smatch m;
            if (regex_match(line, m, regex(R"(^\s*```(\w+)?\s*$)"))) {
                inCodeBlock = true;
                codeLang = m[1].matched ? m[1].str() : "";
                closeLists();
                closeBlockquote();
                string cls = codeLang.empty() ? "" : (" class=\"language-" + codeLang + "\"");
                html.push_back("<pre><code" + cls + ">");
                continue;
            }
        } else {
            // closing fence
            if (regex_match(line, regex(R"(^\s*```\s*$)"))) {
                html.push_back("</code></pre>");
                inCodeBlock = false;
                codeLang.clear();
                continue;
            } else {
                html.push_back(htmlEscape(line));
                html.push_back("\n");
                continue;
            }
        }

        // Horizontal rule
        if (regex_match(line, regex(R"(^\s*([-*_])\s*\1\s*\1\s*$)"))) {
            closeLists();
            closeBlockquote();
            html.push_back("<hr>");
            continue;
        }

        // Blockquote
        if (regex_search(line, regex(R"(^\s*>\s?(.*)$)"))) {
            smatch m;
            regex_match(line, m, regex(R"(^\s*>\s?(.*)$)"));
            string content = processInline(m[1].str());
            if (!inBlockquote) {
                closeLists();
                html.push_back("<blockquote>");
                inBlockquote = true;
            }
            html.push_back("<p>" + content + "</p>");
            continue;
        } else {
            // If previous lines were in blockquote and current is blank, close
            if (inBlockquote && line.empty()) {
                closeBlockquote();
                continue;
            }
        }

        // Headings: # to ######
        smatch hm;
        if (regex_match(line, hm, regex(R"(^\s*(#{1,6})\s+(.*)\s*$)"))) {
            closeLists();
            closeBlockquote();
            int level = (int)hm[1].str().size();
            string content = processInline(hm[2].str());
            html.push_back("<h" + to_string(level) + ">" + content + "</h" + to_string(level) + ">");
            continue;
        }

        // Lists
        // Unordered: -, *
        smatch um;
        if (regex_match(line, um, regex(R"(^\s*[-*]\s+(.*)$)"))) {
            if (!inUl) {
                closeBlockquote();
                if (inOl) { html.push_back("</ol>"); inOl=false; }
                html.push_back("<ul>");
                inUl = true;
            }
            string item = processInline(um[1].str());
            html.push_back("<li>" + item + "</li>");
            continue;
        }

        // Ordered: 1., 2.
        smatch om;
        if (regex_match(line, om, regex(R"(^\s*\d+\.\s+(.*)$)"))) {
            if (!inOl) {
                closeBlockquote();
                if (inUl) { html.push_back("</ul>"); inUl=false; }
                html.push_back("<ol>");
                inOl = true;
            }
            string item = processInline(om[1].str());
            html.push_back("<li>" + item + "</li>");
            continue;
        }

        // Blank line => close lists and emit paragraph separator
        if (line.empty()) {
            closeLists();
            closeBlockquote();
            html.push_back(""); // separator
            continue;
        }

        // Paragraph
        closeBlockquote();
        string content = processInline(line);
        // Join consecutive paragraph lines until blank
        if (!html.empty() && !html.back().empty() && html.back().rfind("<p>",0)==0) {
            // already inside a paragraph? Append with a space
            string last = html.back();
            last.insert(last.size()-4, " " + content); // before </p>
            html.back() = last;
        } else {
            html.push_back("<p>" + content + "</p>");
        }
    }

    // Close any open blocks
    if (inCodeBlock) html.push_back("</code></pre>");
    if (inUl) html.push_back("</ul>");
    if (inOl) html.push_back("</ol>");
    if (inBlockquote) html.push_back("</blockquote>");

    // Wrap with minimal HTML skeleton
    string body;
    for (auto &h : html) {
        body += h;
        if (!h.empty() && h.back() != '\n') body += "\n";
    }
    string outHtml = "<!doctype html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>Converted Markdown</title>\n<style>body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial,sans-serif;max-width:760px;margin:2rem auto;padding:0 1rem;line-height:1.6}code,pre{background:#f6f8fa;border-radius:6px}pre{padding:1rem;overflow:auto}blockquote{border-left:4px solid #ddd;padding-left:1rem;color:#555}hr{border:none;border-top:1px solid #ddd;margin:1.5rem 0}img{max-width:100%;height:auto}</style>\n</head>\n<body>\n" + body + "\n</body>\n</html>\n";

    if (!outputPath.empty()) {
        ofstream out(outputPath);
        if (!out) {
            cerr << "Cannot write output file: " << outputPath << "\n";
            return 1;
        }
        out << outHtml;
        cout << "Wrote HTML to: " << outputPath << "\n";
        return 0;
    }

    // No -o provided: show and offer "type n to download"
    cout << outHtml << "\n";
    cout << "\nType n to download (save) to output.html, or press Enter to exit: ";
    string resp;
    if (!getline(cin, resp)) return 0;
    if (!resp.empty() && (resp == "n" || resp == "N")) {
        string defaultOut = "output.html";
        ofstream out(defaultOut);
        if (!out) {
            cerr << "Cannot write output file: " << defaultOut << "\n";
            return 1;
        }
        out << outHtml;
        cout << "Saved to " << defaultOut << "\n";
    }
    return 0;
}
