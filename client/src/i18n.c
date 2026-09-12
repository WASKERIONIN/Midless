/* v59: localization + UTF-8 text rendering.
 * The English string IS the key; the table below carries ru/zh/ja/ko.
 * Anything not found renders as-is (English), so the game never breaks
 * while the table grows. */
#include "i18n.h"
#include "raylib.h"
#include "rlgl.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int currentLang = LANG_EN;
static Font langFont;
static bool fontLoaded = false;

typedef struct I18nEntry {
    const char *en, *ru, *zh, *ja, *ko;
} I18nEntry;

static const I18nEntry table[] = {
    /* ---- menus ---- */
    { "PAUSED", "ПАУЗА", "暂停", "ポーズ", "일시정지" },
    { "Continue", "Продолжить", "继续", "つづける", "계속하기" },
    { "Options", "Настройки", "设置", "設定", "설정" },
    { "Exit Game", "Выход из игры", "退出游戏", "ゲームをやめる", "게임 종료" },
    { "OPTIONS", "НАСТРОЙКИ", "设置", "設定", "설정" },
    { "Back", "Назад", "返回", "もどる", "뒤로" },
    { "Show Debug: ON", "Отладка: ВКЛ", "调试: 开", "デバッグ: オン", "디버그: 켜짐" },
    { "Show Debug: OFF", "Отладка: ВЫКЛ", "调试: 关", "デバッグ: オフ", "디버그: 꺼짐" },
    { "Fullscreen: ON  (F11)", "Полный экран: ВКЛ (F11)", "全屏: 开 (F11)", "全画面: オン (F11)", "전체 화면: 켜짐 (F11)" },
    { "Fullscreen: OFF  (F11)", "Полный экран: ВЫКЛ (F11)", "全屏: 关 (F11)", "全画面: オフ (F11)", "전체 화면: 꺼짐 (F11)" },
    { "Resolution:", "Разрешение:", "分辨率:", "解像度:", "해상도:" },
    { "VSync: ON  (next launch)", "Вертикальная синх.: ВКЛ (после перезапуска)", "垂直同步: 开 (重启后生效)", "垂直同期: オン (次回起動時)", "수직 동기화: 켜짐 (다음 실행)" },
    { "VSync: OFF  (next launch)", "Вертикальная синх.: ВЫКЛ (после перезапуска)", "垂直同步: 关 (重启后生效)", "垂直同期: オフ (次回起動時)", "수직 동기화: 꺼짐 (다음 실행)" },
    { "Music: ON", "Музыка: ВКЛ", "音乐: 开", "音楽: オン", "음악: 켜짐" },
    { "Music: OFF", "Музыка: ВЫКЛ", "音乐: 关", "音楽: オフ", "음악: 꺼짐" },
    { "Language:", "Язык:", "语言:", "言語:", "언어:" },
    { "Draw Distance: 20 (fixed)", "Дальность прорисовки: 20 (фикс.)", "绘制距离: 20 (固定)", "描画距離: 20 (固定)", "가시 거리: 20 (고정)" },
    /* ---- forge ---- */
    { "WARP CORE FORGE", "КУЗНИЦА ЯДРА", "核心锻造所", "ワープコア鍛冶", "워프 코어 대장간" },
    { "FORGE  5", "КОВКА  5", "锻造 5", "鍛造 5", "제련 5" },
    { "MAX", "МАКС", "满级", "最大", "최대" },
    { "LENS", "ЛИНЗА", "透镜", "レンズ", "렌즈" },
    { "COIL", "КАТУШКА", "线圈", "コイル", "코일" },
    { "BURST", "ЗАЛП", "连发", "バースト", "연발" },
    { "COOL", "ОХЛАД", "冷却", "冷却", "냉각" },
    { "PLATE", "БРОНЯ", "装甲", "装甲", "장갑" },
    { "+6 m laser range", "+6 м дальности лазера", "激光射程 +6米", "レーザー射程 +6m", "레이저 사거리 +6m" },
    { "faster shots", "скорострельность", "更快射击", "連射速度アップ", "더 빠른 발사" },
    { "hold = volleys, endless at max", "удержание = залпы, на максе - без конца", "按住=连射, 满级无限", "長押し=連射, 最大で無限", "길게 누르면 연사, 최대 시 무한" },
    { "cooler coil, quicker shots", "холоднее катушка, быстрее выстрелы", "更快冷却", "クールなコイル, 高速射撃", "더 시원한 코일, 빠른 발사" },
    { "every hit hurts 12% less", "каждый удар слабее на 12%", "每次受击伤害减少12%", "被ダメージ12%減", "피해량 12% 감소" },
    { "B / ESC - close      falling or dying burns out one upgrade", "B / ESC - закрыть      падение или смерть сжигает одну прокачку", "B / ESC - 关闭      坠落或死亡会烧毁一项升级", "B / ESC - 閉じる      落下や死ぬとアップグレードを1つ失う", "B / ESC - 닫기      추락이나 사망 시 업그레이드 1개 소실" },
    /* ---- satchel ---- */
    { "VOID SATCHEL", "ПУСТОТНАЯ СУМКА", "虚空行囊", "ヴォイド袋", "공허 가방" },
    { "carried", "при себе", "携带中", "所持品", "소지품" },
    { "LASER FORGE", "ЛАЗЕР", "激光锻造", "レーザー鍛冶", "레이저 대장간" },
    { "forge at warp cores - B, 5 shards each", "ковка у ядер - B, по 5 осколков", "在核心处锻造 - B, 每个5碎片", "ワープコアで鍛造 - B, 5シャード", "워프 코어에서 제련 - B, 5 파편" },
    { "FIELD LOG", "ЖУРНАЛ", "野外日志", "フィールドログ", "탐사 기록" },
    { "G - eat a mushroom (+3 HP)      E - pick one in the field", "G - съесть гриб (+3 HP)      E - подобрать в поле", "G - 吃蘑菇(+3生命)      E - 采摘蘑菇", "G - キノコを食べる (+3HP)      E - 採る", "G - 버섯 먹기 (+3 HP)      E - 채집" },
    { "Click an item, then a slot below to pin it to 1-4.", "Кликни предмет, затем слот внизу - закрепить за 1-4.", "点击物品, 再点下方格子固定到1-4。", "アイテムをクリックし、下のスロット(1-4)に登録。", "아이템을 클릭한 뒤 아래 슬롯에 지정하세요 (1-4)." },
    { "I / ESC - close", "I / ESC - закрыть", "I / ESC - 关闭", "I / ESC - 閉じる", "I / ESC - 닫기" },
    /* ---- HUD / misc ---- */
    { "CHUNK", "ЧАНК", "区块", "チャンク", "청크" },
    { "M: close", "M: закрыть", "M: 关闭", "M: 閉じる", "M: 닫기" },
    { "M: zoom", "M: увеличить", "M: 放大", "M: 拡大", "M: 확대" },
    { "gaze true:", "истинный взгляд:", "真视:", "真の眼:", "진실한 시선:" },
    /* ---- chat / events ---- */
    { "A violet shell shimmers over the islands... it is raining light.", "Фиолетовая оболочка мерцает над островами... идёт световой дождь.", "紫色的壳在岛屿上空闪烁……光之雨落下了。", "島々に紫の殻がきらめく……光の雨が降る。", "제일 위에 보라색 껍질이 반짝인다…… 빛의 비가 내린다." },
    { "The violet shell returns.", "Фиолетовая оболочка возвращается.", "紫色的壳回来了。", "紫の殻が戻ってきた。", "보라색 껍질이 돌아왔다." },
    { "Void mushroom stored. Press G to eat it (+3 HP).", "Гриб сохранён. Нажми G, чтобы съесть (+3 HP).", "蘑菇已保存。按G食用(+3生命)。", "キノコを保存した。Gで食べる (+3HP)。", "버섯 저장 완료. G를 눌러 먹으세요 (+3 HP)." },
    { "The mushroom hums warmly. HP %d/10. Left: %d.", "Гриб тепло гудит. HP %d/10. Осталось: %d.", "蘑菇温暖地鸣响。生命 %d/10。剩余: %d。", "キノコが温かに鳴る。HP %d/10。残り: %d。", "버섯이 따뜻하게 울린다. HP %d/10. 남음: %d." },
    { "The cocoon splits open. Something many-legged rises.", "Кокон раскрывается. Нечто многоногое поднимается.", "茧裂开了。多足之物站了起来。", "繭が割れる。多脚の何かが現れる。", "고치가 갈라진다. 다리 많은 무언가가 일어선다." },
    { "The cocoon bursts under the beam. Silence... for now.", "Кокон лопается под лучом. Тишина... пока что.", "茧在光束下爆裂。寂静……暂时。", "繭がビームで弾ける。静寂……今のところ。", "고치가 빔 아래 터진다. 조용하다…… 지금은." },
    { "Laser rifle armed.", "Лазерная винтовка заряжена.", "激光步枪已就绪。", "レーザーライフル起動。", "레이저 라이플 장착." },
    { "Blade readied.", "Клинок наготове.", "利刃已出鞘。", "ブレード構え。", "검을 들었다." },
    { "A spell scroll tumbles out of the block! Press 1-4 to read it.", "Из блока выпадает свиток! Нажми 1-4, чтобы прочесть.", "方块中掉出了卷轴! 按1-4阅读。", "ブロックから巻物が出現! 1-4で読む。", "블록에서 두루마리가 굴러 나왔다! 1-4를 눌러 읽으세요." },
    { "The scroll burns - your gaze is true for 25 s. Look into the dark.", "Свиток горит - 25 с твой взгляд истинен. Смотри во тьму.", "卷轴燃烧 - 25秒内你拥有真视。凝视黑暗吧。", "巻物が燃える - 25秒間、真の眼が開く。闇を見よ。", "두루마리가 타오른다 - 25초간 진실한 시선. 어둠을 바라보라." },
    { "Lens reforged - laser range %d m. Shards left: %d.", "Линза перекована - дальность %d м. Осколков: %d.", "透镜重铸 - 射程 %d米。碎片剩余: %d。", "レンズ鍛え直し - 射程 %dm。シャード残り: %d。", "렌즈 재단조 - 사거리 %dm. 파편 남음: %d." },
    { "Coil rewound - faster shots. Shards left: %d.", "Катушка перемотана - выстрелы быстрее. Осколков: %d.", "线圈重绕 - 射速提升。碎片剩余: %d。", "コイル巻き直し - 連射アップ。シャード残り: %d。", "코일 재감기 - 발사 속도 증가. 파편 남음: %d." },
    { "Volley forge: HOLD the trigger for 3-shot bursts. A click is still one shot.", "Залповая ковка: УДЕРЖИВАЙ курок для очереди из 3. Клик - один выстрел.", "连发锻造: 按住扳机进行3连射。单击仍为单发。", "バースト鍛造: トリガー長押しで3連射。クリックは1発。", "연발 제련: 트리거를 길게 눌러 3점사. 클릭은 한 발." },
    { "Volley forge: bursts of five now. Clicks stay single shots.", "Залповая ковка: очереди по пять. Клик - один выстрел.", "连发锻造: 现在为5连射。单击仍为单发。", "バースト鍛造: 5連射に。クリックは1発のまま。", "연발 제련: 이제 5점사. 클릭은 한 발 그대로." },
    { "The coil sings: hold for endless shots - pauses cool it down.", "Катушка поёт: удерживай для бесконечных выстрелов - паузы охлаждают.", "线圈歌唱: 按住可无限射击 - 间歇可冷却。", "コイルが歌う: 長押しで無限射撃 - 合間に冷却。", "코일이 노래한다: 길게 눌러 무한 발사 - 휴식 시 냉각." },
    { "Cooling coils seated - the laser runs cooler and shoots a touch faster.", "Охлаждающие катушки установлены - лазер холоднее и чуть быстрее.", "冷却线圈就位 - 激光更冷, 射速微升。", "冷却コイル装着 - レーザーが冷えて少し速く。", "냉각 코일 장착 - 레이저가 더 시원하고 약간 빨라진다." },
    { "Coolant loops online - pauses drain the heat much faster.", "Контуры охлаждения активны - паузы снижают нагрев гораздо быстрее.", "冷却回路上线 - 间歇大幅加速散热。", "冷却ループ起動 - 合間の冷却が大幅アップ。", "냉각 회로 가동 - 휴식 시 열 배출 대폭 증가." },
    { "Arctic coil: the laser shrugs off heat. Hold as long as you dare.", "Арктическая катушка: лазеру жар не страшен. Держи, пока не страшно.", "极地线圈: 激光无视热量。敢按多久按多久。", "極地コイル: レーザーは熱を嫌わない。思いのまま長押しを。", "북극 코일: 레이저가 열을 무시한다. 감당할 만큼 누르세요." },
    { "Your armor is fully forged.", "Броня выкована полностью.", "装甲已锻造完毕。", "装甲は完全に鍛えられた。", "장갑이 완전히 단조되었다." },
    { "Armor needs %d shards. Fell hunters, crawlers, wisps, spiders.", "Броне нужно %d осколков. Вали охотников, ползунов, огоньков, пауков.", "装甲需要%d碎片。击杀猎手、爬行者、幽灵、蜘蛛。", "装甲には%dシャード必要。ハンターらを倒せ。", "장갑에 %d 파편 필요. 사냥꾼, 거미 등을 처치하세요." },
    { "Void plate seated - hits hurt 12% less.", "Пустотная пластина встала - удары слабее на 12%.", "虚空板甲就位 - 受击伤害减少12%。", "ヴォイド板装着 - 被弾12%減。", "공허 판금 장착 - 피해 12% 감소." },
    { "Double plate - hits hurt 24% less.", "Двойная пластина - удары слабее на 24%.", "双层板甲 - 受击伤害减少24%。", "二重板 - 被弾24%減。", "이중 판금 - 피해 24% 감소." },
    { "Aegis plating - hits hurt 36% less.", "Эгидная обшивка - удары слабее на 36%.", "神盾装甲 - 受击伤害减少36%。", "イージス装甲 - 被弾36%減。", "이지스 장갑 - 피해 36% 감소." },
    { "No mushrooms in the satchel.", "В сумке нет грибов.", "行囊里没有蘑菇。", "袋にキノコがない。", "가방에 버섯이 없다." },
    { "No spell scrolls.", "Нет свитков.", "没有卷轴。", "巻物がない。", "두루마리가 없다." },
    { "HP is already full.", "Здоровье уже полное.", "生命值已满。", "HPは既に満タン。", "이미 체력이 가득하다." },
    { "Glowmoths shimmer between the islands.", "Светлячки-мотыльки мерцают между островами.", "发光的飞蛾在岛屿间闪烁。", "光の蛾が島々の間できらめく。", "빛나는 나방이 섬 사이에서 반짝인다." },
    { "The launch pad hurls you into the void. Glide!", "Стартовая площадка швыряет тебя в пустоту. Планируй!", "发射台将你抛入虚空。滑翔吧！", "発射台が虚空へ放り出す。滑翔せよ！", "발사대가 공허로 내던진다. 활공하세요!" },
    { "The void lets you go. Returned to the starter island.", "Пустота отпускает тебя. Возвращение на стартовый остров.", "虚空放你离开。返回起始岛。", "虚空が解放する。初期の島へ戻る。", "공허가 당신을 놓아준다. 시작 섬으로 복귀." },
    { "An upgrade needs %d shards. Fell hunters, crawlers, wisps, spiders.", "Прокачке нужно %d осколков. Вали охотников, ползунов, огоньков, пауков.", "升级需要%d碎片。击杀猎手、爬行者、幽灵、蜘蛛。", "アップグレードには%dシャード必要。", "업그레이드에 %d 파편 필요." },
    { "That part of the laser is already maxed.", "Эта часть лазера уже на максимуме.", "该激光部件已满级。", "その部分は既に最大。", "그 부분은 이미 최대치다." },
    { "The core hums: your laser is fully forged.", "Ядро гудит: лазер выкован полностью.", "核心嗡鸣: 激光已完全锻造。", "コアが唸る: レーザーは完全に鍛えられた。", "코어가 울린다: 레이저가 완전히 단조되었다." },
    { "Shards left: %d.", "Осколков осталось: %d.", "碎片剩余: %d。", "シャード残り: %d。", "파편 남음: %d." },
    { "The hunters got you. Wake up on the starter island.", "Охотники достали тебя. Пробуждение на стартовом острове.", "猎手抓住了你。在起始岛醒来。", "ハンターに捕まった。初期の島で目覚める。", "사냥꾼에게 당했다. 시작 섬에서 눈을 뜬다." },
};

#define TABLE_N (int)(sizeof(table) / sizeof(table[0]))

/* per-language codepoint sets */
static int *codepoints = NULL;
static int cpCount = 0;

static void AddRange(int from, int to) {
    for (int c = from; c <= to; c++) codepoints[cpCount++] = c;
}

static const char *FontFileFor(int lang) {
    return (lang == LANG_KO) ? "NanumGothic.ttf" : "NotoSansSC-Regular.ttf";
}

static void UnloadCp(void) { if (codepoints) { free(codepoints); codepoints = NULL; } cpCount = 0; }

static void BuildCodepoints(int lang) {
    UnloadCp();
    int cap = 4096;
    if (lang == LANG_ZH || lang == LANG_JA) cap = 22000;
    if (lang == LANG_KO) cap = 12500;
    codepoints = (int *)malloc(sizeof(int) * cap);
    cpCount = 0;
    AddRange(0x20, 0x7E);          /* ASCII */
    AddRange(0x2010, 0x2027);      /* dashes, quotes, ellipsis */
    AddRange(0x20AC, 0x20AC);
    if (lang == LANG_RU || lang == LANG_ZH || lang == LANG_JA || lang == LANG_KO)
        AddRange(0x0400, 0x04FF);  /* cyrillic (the fonts carry it) */
    if (lang == LANG_ZH) {
        AddRange(0x3000, 0x303F); AddRange(0x4E00, 0x9FA5); AddRange(0xFF00, 0xFF65);
    } else if (lang == LANG_JA) {
        AddRange(0x3000, 0x30FF); AddRange(0x4E00, 0x9FA5); AddRange(0xFF00, 0xFF65);
    } else if (lang == LANG_KO) {
        AddRange(0x1100, 0x11FF); AddRange(0x3130, 0x318F); AddRange(0xAC00, 0xD7A3);
    }
}

static void LoadLangFont(int lang) {
    if (fontLoaded) { UnloadFont(langFont); fontLoaded = false; }
    BuildCodepoints(lang);
    const char *file = FontFileFor(lang);
    char path[512];
    /* packaged layout: <exe>/fonts/<file>; dev tree: client/fonts/<file> */
    snprintf(path, sizeof(path), "%sfonts/%s", GetApplicationDirectory(), file);
    if (!FileExists(path)) snprintf(path, sizeof(path), "client/fonts/%s", file);
    if (FileExists(path)) {
        SetTraceLogLevel(LOG_WARNING);
        langFont = LoadFontEx(path, 32, codepoints, cpCount);
        SetTextureFilter(langFont.texture, TEXTURE_FILTER_BILINEAR);
        SetTraceLogLevel(LOG_INFO);
        fontLoaded = true;
    }
    UnloadCp();
    if (!fontLoaded) langFont = GetFontDefault();
}

void I18n_Init(void) { LoadLangFont(currentLang); }
void I18n_Shutdown(void) {
    if (fontLoaded) { UnloadFont(langFont); fontLoaded = false; }
    UnloadCp();
}
void I18n_SetLanguage(int lang) {
    if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
    if (lang == currentLang && fontLoaded) return;
    currentLang = lang;
    LoadLangFont(currentLang);
}
int I18n_GetLanguage(void) { return currentLang; }

const char *I18n_LangLabel(int lang) {
    switch (lang) {
        case LANG_RU: return "Русский";
        case LANG_ZH: return "中文";
        case LANG_JA: return "日本語";
        case LANG_KO: return "한국어";
        default: return "English";
    }
}

const char *Tr(const char *s) {
    if (currentLang == LANG_EN || s == NULL) return s;
    for (int i = 0; i < TABLE_N; i++) {
        if (strcmp(table[i].en, s) == 0) {
            const char *out = NULL;
            switch (currentLang) {
                case LANG_RU: out = table[i].ru; break;
                case LANG_ZH: out = table[i].zh; break;
                case LANG_JA: out = table[i].ja; break;
                case LANG_KO: out = table[i].ko; break;
            }
            return (out && out[0]) ? out : s;
        }
    }
    return s;
}

Font I18n_Font(void) { return fontLoaded ? langFont : GetFontDefault(); }

static Vector2 DrawImpl(const char *text, Vector2 pos, int size, Color tint) {
    Font f = I18n_Font();
    float sz = (float)size;
    Vector2 bounds = MeasureTextEx(f, text, sz, 0.0f);
    if (f.texture.id != GetFontDefault().texture.id) {
        DrawTextEx(f, text, pos, sz, 0.0f, tint);
    } else {
        DrawText(text, (int)pos.x, (int)pos.y, size, tint);  /* fallback: ascii path */
        return (Vector2){ (float)MeasureText(text, size), (float)size };
    }
    return bounds;
}

void I18n_DrawText(const char *text, int x, int y, int size, Color tint) {
    DrawImpl(Tr(text), (Vector2){ (float)x, (float)y }, size, tint);
}

float I18n_MeasureText(const char *text, int size) {
    Font f = I18n_Font();
    if (f.texture.id == GetFontDefault().texture.id) return (float)MeasureText(text, size);
    return MeasureTextEx(f, Tr(text), (float)size, 0.0f).x;
}

Vector2 I18n_MeasureEx(const char *text, int size) {
    Font f = I18n_Font();
    if (f.texture.id == GetFontDefault().texture.id)
        return (Vector2){ (float)MeasureText(text, size), (float)size };
    return MeasureTextEx(f, Tr(text), (float)size, 0.0f);
}

void I18n_DrawEx(const char *text, Vector2 pos, int size, Color tint) {
    DrawImpl(Tr(text), pos, size, tint);
}
