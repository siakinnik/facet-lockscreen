// Lock screen for Facet: a PIN, a password or a pattern. The core locks the
// device (at start, when the screen goes off) and shows only this module's
// lock screen until it says unlock(); the secret is checked here and never
// leaves this process, and only its salted hash is stored (secret.h).
#include <algorithm>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

#include "facet/plugin.h"
#include "i18n/i18n.h"
#include "secret.h"

using facet::Json;
using facet::sdk::Canvas;
using facet::sdk::Plugin;
using facet::sdk::Screen;
using ls::Kind;

#ifndef LS_VERSION
#define LS_VERSION "0.0.0-dev"
#endif

namespace {

constexpr int kMaxPin = 16;

void wipe(std::string& s) {
    std::fill(s.begin(), s.end(), '\0');
    s.clear();
}

int64_t wall_now() { return int64_t(std::time(nullptr)); }

// What is being typed or drawn: the PIN, the password or the pattern.
struct Entry {
    std::string pin, password;
    std::vector<int> dots;
    bool dragging = false;
    float fx = 0, fy = 0;  // the finger while drawing a pattern

    void clear() {
        wipe(pin);
        wipe(password);
        dots.clear();
        dragging = false;
    }
    std::string value(Kind k) const {
        if (k == Kind::Pin) return pin;
        if (k == Kind::Password) return password;
        if (k == Kind::Pattern) return ls::Secret::pattern_text(dots);
        return {};
    }
};

class LockScreen {
public:
    explicit LockScreen(Plugin& p) : plugin_(p), secret_(p.data_dir().empty() ? "." : p.data_dir()) {}

    void start() {
        secret_ = ls::Secret(plugin_.data_dir());
        secret_.load();
        plugin_.lock_config(secret_.kind() != Kind::None);
        refresh();
    }

    void on_locked(bool locked) {
        locked_ = locked;
        lock_.clear();
        message_.clear();
        refresh();
    }

    void on_tick() {
        // The clock, and the countdown after too many wrong attempts.
        std::time_t now = std::time(nullptr);
        int minute = int(now / 60);
        int wait = secret_.wait_seconds(wall_now());
        if (minute != minute_ || wait != wait_shown_) refresh();
    }

    void on_event(const std::string& id, const Json& v) {
        if (id.rfind("lk_", 0) == 0) {
            if (!locked_) return;
            input(lock_, secret_.kind(), id.substr(3), v, true);
        } else if (id.rfind("st_", 0) == 0) {
            std::string what = id.substr(3);
            if (what == "cancel") {
                step_ = Step::Idle;
                setup_.clear();
                wipe(first_);
                message_settings_.clear();
            } else if (step_ == Step::Idle) {
                start_flow(what);
            } else {
                input(setup_, step_kind(), what, v, false);
            }
        }
        refresh();
    }

    void on_submit(const std::string& id, const std::string& text) {
        if (id == "lk_pw" && locked_) {
            lock_.password = text;
            submit(lock_, secret_.kind(), true);
        } else if (id == "st_pw" && step_ != Step::Idle) {
            setup_.password = text;
            submit(setup_, step_kind(), false);
        }
        refresh();
    }

    void refresh() {
        minute_ = int(std::time(nullptr) / 60);
        wait_shown_ = secret_.wait_seconds(wall_now());
        if (locked_) plugin_.set_lock_ui(lock_screen());
        plugin_.set_ui(settings());
    }

private:
    enum class Step { Idle, Current, New, Confirm };

    // ---------------------------------------------------------------- input

    // A tap on the PIN pad, a change of the password field, a finger on the
    // pattern, or OK.
    void input(Entry& e, Kind kind, const std::string& what, const Json& v, bool lock) {
        if (what == "pad") {
            const std::string& key = v.str();
            if (key == "del") {
                if (!e.pin.empty()) e.pin.pop_back();
            } else if (key == "ok") {
                submit(e, kind, lock);
            } else if (key.size() == 1 && key[0] >= '0' && key[0] <= '9' && e.pin.size() < size_t(kMaxPin)) {
                e.pin += key;
            }
        } else if (what == "pw") {
            e.password = v.str();
        } else if (what == "ok") {
            submit(e, kind, lock);
        } else if (what == "pattern" && v.is_object()) {
            const std::string& phase = v["phase"].str();
            float x = float(v["x"].as_number()), y = float(v["y"].as_number());
            if (phase == "down") {
                e.dots.clear();
                e.dragging = true;
            }
            if (!e.dragging) return;
            e.fx = x, e.fy = y;
            touch_dot(e, x, y);
            if (phase == "up") {
                e.dragging = false;
                if (e.dots.size() < 4) {
                    (lock ? message_ : message_settings_) = plugin_.tr("Connect at least 4 dots");
                    e.dots.clear();
                } else {
                    submit(e, kind, lock);
                }
            }
        }
    }

    // The dot under the finger joins the pattern; a dot passed over on the
    // way from the last one (the middle of a row) joins first, as on phones.
    void touch_dot(Entry& e, float x, float y) {
        Grid g = grid();
        for (int i = 0; i < 9; ++i) {
            float cx = g.x0 + float(i % 3) * g.step, cy = g.y0 + float(i / 3) * g.step;
            if (std::hypot(x - cx, y - cy) > g.step * 0.33f) continue;
            if (std::find(e.dots.begin(), e.dots.end(), i) != e.dots.end()) return;
            if (!e.dots.empty()) {
                int a = e.dots.back();
                int ar = a / 3, ac = a % 3, br = i / 3, bc = i % 3;
                if ((ar + br) % 2 == 0 && (ac + bc) % 2 == 0) {
                    int mid = (ar + br) / 2 * 3 + (ac + bc) / 2;
                    if (std::find(e.dots.begin(), e.dots.end(), mid) == e.dots.end()) e.dots.push_back(mid);
                }
            }
            e.dots.push_back(i);
            return;
        }
    }

    void submit(Entry& e, Kind kind, bool lock) {
        std::string value = e.value(kind);
        if (lock) {
            switch (secret_.check(value, wall_now())) {
                case ls::Secret::Result::Ok:
                    message_.clear();
                    plugin_.unlock();
                    break;
                case ls::Secret::Result::Wrong: message_ = wrong(kind); break;
                case ls::Secret::Result::Wait: break;  // the countdown says it
            }
            e.clear();
            wipe(value);
            return;
        }
        // Settings: the current secret, then the new one twice.
        std::string& msg = message_settings_;
        if (step_ == Step::Current) {
            auto r = secret_.check(value, wall_now());
            if (r == ls::Secret::Result::Ok) {
                msg.clear();
                if (target_ == Kind::None) {
                    secret_.set(Kind::None, {});
                    plugin_.lock_config(false);
                    msg = plugin_.tr("The lock is off.");
                    step_ = Step::Idle;
                } else {
                    step_ = Step::New;
                }
            } else if (r == ls::Secret::Result::Wrong) {
                msg = wrong(kind);
            }
        } else if (step_ == Step::New) {
            if (!ls::Secret::valid(kind, value)) {
                msg = kind == Kind::Pin        ? plugin_.tr("A PIN has 4 to 16 digits.")
                      : kind == Kind::Password ? plugin_.tr("A password has at least 4 characters.")
                                               : plugin_.tr("Connect at least 4 dots");
            } else {
                first_ = value;
                msg.clear();
                step_ = Step::Confirm;
            }
        } else if (step_ == Step::Confirm) {
            if (ls::equal_ct(value, first_) && secret_.set(target_, value)) {
                plugin_.lock_config(true);
                msg = plugin_.tr("The lock is set up. It locks when the screen goes off.");
                step_ = Step::Idle;
            } else {
                msg = plugin_.tr("They do not match. Try again.");
                step_ = Step::New;
            }
            wipe(first_);
        }
        e.clear();
        wipe(value);
    }

    std::string wrong(Kind kind) const {
        std::string what = kind == Kind::Pin        ? plugin_.tr("Wrong PIN.")
                           : kind == Kind::Password ? plugin_.tr("Wrong password.")
                                                    : plugin_.tr("Wrong pattern.");
        int left = secret_.attempts_left();
        if (left > 0 && left <= 2) what += " " + plugin_.tr("Attempts left before a pause: {}", {std::to_string(left)});
        return what;
    }

    void start_flow(const std::string& what) {
        Kind k = what == "set_pin" ? Kind::Pin : what == "set_password" ? Kind::Password
                 : what == "set_pattern" ? Kind::Pattern : Kind::None;
        if (what == "lock_now") {
            plugin_.lock_now();
            return;
        }
        if (k == Kind::None && what != "remove") return;
        target_ = k;
        setup_.clear();
        message_settings_.clear();
        step_ = secret_.kind() != Kind::None ? Step::Current : Step::New;
    }

    Kind step_kind() const { return step_ == Step::Current ? secret_.kind() : target_; }

    // ---------------------------------------------------------------- drawing

    float width() const { return float(std::max(240, plugin_.content_width())); }

    struct Grid {
        float x0, y0, step, size;
    };
    Grid grid() const {
        float size = std::min(width(), 320.f);
        float step = size / 3;
        float x0 = (width() - size) / 2 + step / 2;
        return {x0, step / 2, step, size};
    }

    Canvas clock() const {
        Canvas c;
        std::time_t now = std::time(nullptr);
        std::tm tm{};
        localtime_r(&now, &tm);
        char hm[8];
        std::snprintf(hm, sizeof hm, "%02d:%02d", tm.tm_hour, tm.tm_min);
        static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        static const char* months[] = {"January", "February", "March",     "April",   "May",      "June",
                                       "July",    "August",   "September", "October", "November", "December"};
        float w = width();
        c.text(0, 4, w, 76, hm, 64, "text", "center", "light");
        c.text(0, 82, w, 28,
               plugin_.tr("{}, {} {}", {plugin_.tr(days[tm.tm_wday]), std::to_string(tm.tm_mday),
                                        plugin_.tr(months[tm.tm_mon])}),
               18, "text_dim", "center");
        return c;
    }

    Canvas line(const std::string& text, const std::string& color) const {
        Canvas c;
        c.text(0, 0, width(), 32, text, 16, color, "center");
        return c;
    }

    Canvas pin_dots(const Entry& e) const {
        Canvas c;
        float w = width(), gap = 26;
        size_t n = std::max<size_t>(e.pin.size(), 4);
        float x0 = w / 2 - gap * float(n - 1) / 2;
        for (size_t i = 0; i < n; ++i) {
            float cx = x0 + gap * float(i);
            if (i < e.pin.size()) c.circle(cx, 20, 8, "accent");
            else c.ring(cx, 20, 8, 2, "track");
        }
        return c;
    }

    Canvas keypad(bool enabled, float& height) const {
        Canvas c;
        const float gap = 12, w = width();
        float key = std::min(96.f, (w - 2 * gap) / 3), kh = key * 0.72f;
        float x0 = (w - (3 * key + 2 * gap)) / 2;
        const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "del", "0", "ok"};
        for (int i = 0; i < 12; ++i) {
            float x = x0 + float(i % 3) * (key + gap), y = float(i / 3) * (kh + gap);
            std::string k = keys[i];
            bool ok = k == "ok";
            c.rrect(x, y, key, kh, 18, ok ? "accent" : "surface", enabled ? k : std::string(),
                    ok ? std::string() : "surface_pressed");
            const char* fg = !enabled ? "text_dim" : ok ? "on_accent" : "text";
            if (k == "del") c.icon(x + key / 2 - 14, y + kh / 2 - 14, 28, "backspace", fg);
            else if (ok) c.icon(x + key / 2 - 14, y + kh / 2 - 14, 28, "enter", fg);
            else c.text(x, y, key, kh, k, 30, fg, "center", "light");
        }
        height = 4 * (kh + gap) - gap;
        return c;
    }

    Canvas pattern(const Entry& e, bool enabled) const {
        Canvas c;
        Grid g = grid();
        auto at = [&](int i) { return std::pair<float, float>{g.x0 + float(i % 3) * g.step, g.y0 + float(i / 3) * g.step}; };
        for (size_t i = 1; i < e.dots.size(); ++i) {
            auto [ax, ay] = at(e.dots[i - 1]);
            auto [bx, by] = at(e.dots[i]);
            c.line(ax, ay, bx, by, 6, "accent");
        }
        if (e.dragging && !e.dots.empty()) {
            auto [ax, ay] = at(e.dots.back());
            c.line(ax, ay, e.fx, e.fy, 6, "accent");
        }
        for (int i = 0; i < 9; ++i) {
            auto [cx, cy] = at(i);
            bool on = std::find(e.dots.begin(), e.dots.end(), i) != e.dots.end();
            if (on) {
                c.ring(cx, cy, 22, 3, "accent");
                c.circle(cx, cy, 10, "accent");
            } else {
                c.circle(cx, cy, 8, enabled ? "text_dim" : "track");
            }
        }
        return c;
    }

    // The input for `kind`, with canvas and widget ids starting with `prefix`.
    void entry(Screen& s, const std::string& prefix, Kind kind, const Entry& e, bool enabled) const {
        if (kind == Kind::Pin) {
            s.canvas(prefix + "dots", 40, pin_dots(e));
            float h = 0;
            Canvas pad = keypad(enabled, h);
            s.canvas(prefix + "pad", h, pad);
        } else if (kind == Kind::Password) {
            if (enabled) {
                s.text_field(prefix + "pw", plugin_.tr("Password"), e.password, plugin_.tr("Tap to type"), true);
                s.button(prefix + "ok", plugin_.tr("OK"), "primary");
            }
        } else if (kind == Kind::Pattern) {
            s.canvas(prefix + "pattern", grid().size, pattern(e, enabled), enabled);
        }
    }

    Screen lock_screen() const {
        Screen s("");
        s.canvas("lk_clock", 116, clock());
        int wait = secret_.wait_seconds(wall_now());
        Kind kind = secret_.kind();
        std::string msg = wait > 0 ? plugin_.tr("Too many attempts. Try again in {} s.", {std::to_string(wait)})
                          : !message_.empty() ? message_
                          : kind == Kind::Pin      ? plugin_.tr("Enter the PIN")
                          : kind == Kind::Password ? plugin_.tr("Enter the password")
                                                   : plugin_.tr("Draw the pattern");
        s.canvas("lk_msg", 32, line(msg, wait > 0 || !message_.empty() ? "warn" : "text_dim"));
        entry(s, "lk_", kind, lock_, wait == 0);
        return s;
    }

    Screen settings() const {
        Screen s(plugin_.tr("Screen lock"));
        Kind kind = secret_.kind();
        if (step_ == Step::Idle) {
            s.section(plugin_.tr("Screen lock"));
            s.info(plugin_.tr("Lock"), kind_label(kind), kind == Kind::None ? "dim" : "good");
            if (!message_settings_.empty()) s.note(message_settings_);
            s.button("st_set_pin", kind == Kind::Pin ? plugin_.tr("Change the PIN") : plugin_.tr("Use a PIN"));
            s.button("st_set_password",
                     kind == Kind::Password ? plugin_.tr("Change the password") : plugin_.tr("Use a password"));
            s.button("st_set_pattern",
                     kind == Kind::Pattern ? plugin_.tr("Change the pattern") : plugin_.tr("Use a pattern"));
            if (kind != Kind::None) {
                s.button("st_lock_now", plugin_.tr("Lock now"), "primary");
                s.button("st_remove", plugin_.tr("Turn the lock off"), "danger");
            }
            s.note(plugin_.tr("The device locks when the screen goes off and at start. Only a salted hash of "
                              "the secret is stored. After 5 wrong attempts each next one has to wait, longer "
                              "every time."));
            s.note(plugin_.tr("Forgot it? With administrator access to the device, delete lock.json in this "
                              "module's data folder."));
            return s;
        }
        Kind k = step_kind();
        std::string title;
        if (step_ == Step::Current)
            title = k == Kind::Pin ? plugin_.tr("Enter your current PIN") : k == Kind::Password
                        ? plugin_.tr("Enter your current password") : plugin_.tr("Draw your current pattern");
        else if (step_ == Step::New)
            title = k == Kind::Pin ? plugin_.tr("Choose a PIN") : k == Kind::Password ? plugin_.tr("Choose a password")
                                                                                       : plugin_.tr("Draw a new pattern");
        else
            title = k == Kind::Pin ? plugin_.tr("Repeat the PIN") : k == Kind::Password ? plugin_.tr("Repeat the password")
                                                                                         : plugin_.tr("Draw the pattern again");
        s.section(title);
        int wait = step_ == Step::Current ? secret_.wait_seconds(wall_now()) : 0;
        if (wait > 0) s.info(plugin_.tr("Too many attempts"), plugin_.tr("wait {} s", {std::to_string(wait)}), "warn");
        else if (!message_settings_.empty()) s.note(message_settings_);
        entry(s, "st_", k, setup_, wait == 0);
        s.button("st_cancel", plugin_.tr("Cancel"));
        return s;
    }

    std::string kind_label(Kind k) const {
        switch (k) {
            case Kind::Pin: return plugin_.tr("PIN");
            case Kind::Password: return plugin_.tr("Password");
            case Kind::Pattern: return plugin_.tr("Pattern");
            default: return plugin_.tr("None");
        }
    }

    Plugin& plugin_;
    ls::Secret secret_;
    bool locked_ = false;
    Entry lock_, setup_;
    std::string message_, message_settings_;
    Step step_ = Step::Idle;
    Kind target_ = Kind::None;
    std::string first_;  // the new secret, until it is repeated
    int minute_ = -1, wait_shown_ = -1;
};

}  // namespace

int main() {
    Plugin plugin("lockscreen", LS_VERSION);
    ls::register_translations(plugin.catalog());
    LockScreen app(plugin);
    plugin.on_hello = [&](const Json&) { app.start(); };
    plugin.on_locked = [&](bool locked) { app.on_locked(locked); };
    plugin.on_event = [&](const std::string& id, const Json& v) { app.on_event(id, v); };
    plugin.on_submit = [&](const std::string& id, const std::string& text) { app.on_submit(id, text); };
    plugin.on_locale = [&](const std::string&) { app.refresh(); };
    plugin.on_layout = [&](int) { app.refresh(); };
    plugin.on_tick = [&] { app.on_tick(); };
    return plugin.run(1000);
}
