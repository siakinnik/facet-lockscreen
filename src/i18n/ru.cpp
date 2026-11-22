// Russian translation of the lock screen module. Keys are the English source strings.
#include "i18n/i18n.h"

namespace ls {

namespace {

const facet::i18n::Table& ru() {
    static const facet::i18n::Table table = {
        // Lock screen
        {"Enter the PIN", "Введите PIN-код"},
        {"Enter the password", "Введите пароль"},
        {"Draw the pattern", "Нарисуйте графический ключ"},
        {"Wrong PIN.", "Неверный PIN-код."},
        {"Wrong password.", "Неверный пароль."},
        {"Wrong pattern.", "Неверный графический ключ."},
        {"Attempts left before a pause: {}", "Попыток до паузы: {}"},
        {"Too many attempts. Try again in {} s.", "Слишком много попыток. Повторите через {} с."},
        {"Connect at least 4 dots", "Соедините хотя бы 4 точки"},
        {"Password", "Пароль"},
        {"Tap to type", "Нажмите, чтобы ввести"},
        {"OK", "ОК"},

        // Date: "{weekday}, {day} {month}"
        {"{}, {} {}", "{}, {} {}"},
        {"Sunday", "воскресенье"},
        {"Monday", "понедельник"},
        {"Tuesday", "вторник"},
        {"Wednesday", "среда"},
        {"Thursday", "четверг"},
        {"Friday", "пятница"},
        {"Saturday", "суббота"},
        {"January", "января"},
        {"February", "февраля"},
        {"March", "марта"},
        {"April", "апреля"},
        {"May", "мая"},
        {"June", "июня"},
        {"July", "июля"},
        {"August", "августа"},
        {"September", "сентября"},
        {"October", "октября"},
        {"November", "ноября"},
        {"December", "декабря"},

        // Settings
        {"Screen lock", "Блокировка экрана"},
        {"Lock", "Блокировка"},
        {"PIN", "PIN-код"},
        {"Pattern", "Графический ключ"},
        {"None", "Нет"},
        {"Use a PIN", "Использовать PIN-код"},
        {"Use a password", "Использовать пароль"},
        {"Use a pattern", "Использовать графический ключ"},
        {"Change the PIN", "Сменить PIN-код"},
        {"Change the password", "Сменить пароль"},
        {"Change the pattern", "Сменить графический ключ"},
        {"Lock now", "Заблокировать сейчас"},
        {"Turn the lock off", "Отключить блокировку"},
        {"Enter your current PIN", "Введите текущий PIN-код"},
        {"Enter your current password", "Введите текущий пароль"},
        {"Draw your current pattern", "Нарисуйте текущий графический ключ"},
        {"Choose a PIN", "Придумайте PIN-код"},
        {"Choose a password", "Придумайте пароль"},
        {"Draw a new pattern", "Нарисуйте новый графический ключ"},
        {"Repeat the PIN", "Повторите PIN-код"},
        {"Repeat the password", "Повторите пароль"},
        {"Draw the pattern again", "Нарисуйте ключ ещё раз"},
        {"A PIN has 4 to 16 digits.", "В PIN-коде от 4 до 16 цифр."},
        {"A password has at least 4 characters.", "В пароле хотя бы 4 символа."},
        {"They do not match. Try again.", "Не совпадает. Попробуйте ещё раз."},
        {"The lock is set up. It locks when the screen goes off.",
         "Блокировка настроена. Устройство блокируется, когда гаснет экран."},
        {"The lock is off.", "Блокировка отключена."},
        {"Too many attempts", "Слишком много попыток"},
        {"wait {} s", "подождите {} с"},
        {"Cancel", "Отмена"},
        {"The device locks when the screen goes off and at start. Only a salted hash of the secret is stored. "
         "After 5 wrong attempts each next one has to wait, longer every time.",
         "Устройство блокируется, когда гаснет экран, и при запуске. Хранится только солёный хеш секрета. После "
         "5 неверных попыток каждая следующая ждёт, и с каждым разом дольше."},
        {"Forgot it? With administrator access to the device, delete lock.json in this module's data folder.",
         "Забыли? С правами администратора удалите lock.json в папке данных этого модуля."},
    };
    return table;
}

}  // namespace

void register_translations(facet::i18n::Catalog& catalog) { catalog.add("ru", ru()); }

}  // namespace ls
