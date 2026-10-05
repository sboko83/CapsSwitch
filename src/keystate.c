#include "keystate.h"

const int ks_physical_modifiers[KS_PHYSICAL_MODIFIER_COUNT] = {
    KS_VK_LSHIFT, KS_VK_RSHIFT, KS_VK_LCONTROL, KS_VK_RCONTROL,
    KS_VK_LMENU, KS_VK_RMENU, KS_VK_LWIN, KS_VK_RWIN
};

/* Every key that counts as a modifier; bit i of ks_state.modifiers is
   modifier_keys[i]. Generic codes are kept too in case a sender uses them. */
static const int modifier_keys[] = {
    KS_VK_SHIFT, KS_VK_CONTROL, KS_VK_MENU, KS_VK_LWIN, KS_VK_RWIN,
    KS_VK_LSHIFT, KS_VK_RSHIFT, KS_VK_LCONTROL, KS_VK_RCONTROL, KS_VK_LMENU, KS_VK_RMENU
};
#define MODIFIER_KEY_COUNT ((int)(sizeof modifier_keys / sizeof modifier_keys[0]))

static int modifier_bit(int key) {
    for (int i = 0; i < MODIFIER_KEY_COUNT; i++)
        if (modifier_keys[i] == key) return i;
    return -1;
}

void ks_init(ks_state *state, ks_is_down_fn is_down) {
    state->modifiers = 0;
    state->caps_held = false;
    state->suppress_press = false;
    state->enabled = true;
    state->is_down = is_down;
}

bool ks_is_modifier(int key) {
    return modifier_bit(key) >= 0;
}

static void drop_released_modifiers(ks_state *state) {
    for (int i = 0; i < MODIFIER_KEY_COUNT; i++)
        if ((state->modifiers & (1u << i)) && !state->is_down(modifier_keys[i]))
            state->modifiers &= ~(1u << i);
}

ks_action ks_process(ks_state *state, int key, bool down) {
    int bit = modifier_bit(key);
    if (bit >= 0) {
        if (down) state->modifiers |= 1u << bit;
        else state->modifiers &= ~(1u << bit);
        return KS_PASS;
    }
    if (key != KS_VK_CAPITAL) return KS_PASS;
    if (!down) {
        bool suppress = state->caps_held && state->suppress_press;
        state->caps_held = false;
        state->suppress_press = false;
        return suppress ? KS_SUPPRESS : KS_PASS;
    }
    if (state->caps_held) return state->suppress_press ? KS_SUPPRESS : KS_PASS;
    state->caps_held = true;
    if (state->is_down) drop_released_modifiers(state);
    state->suppress_press = state->enabled && state->modifiers == 0;
    return state->suppress_press ? KS_SWITCH : KS_PASS;
}
