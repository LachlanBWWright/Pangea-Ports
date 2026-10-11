#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "TouchStick.h"
#include "TouchLayout.h"

static void near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.00002f);
}

static void test_stick(void)
{
    const float travel = 75.0f;
    for (int angle = 0; angle < 360; angle += 7)
    {
        float radians = angle * 0.01745329252f;
        for (int step = 1; step <= 150; step++)
        {
            float distance = step * 0.75f;
            float dx = cosf(radians) * distance;
            float dy = sinf(radians) * distance;
            PangeaTouchStick stick = PangeaTouchStick_Update(100, 200, 100, 25, 100 + dx, 200 + dy);
            float expected = fminf(distance / travel, 1.0f);
            near(hypotf(stick.x, stick.y), expected);
            near((stick.knobX - 100) / travel, stick.x);
            near((stick.knobY - 200) / travel, stick.y);
            assert(hypotf(stick.knobX - 100, stick.knobY - 200) <= travel + 0.001f);
        }
    }
    PangeaTouchStick tiny = PangeaTouchStick_Update(0, 0, 100, 25, 0.00001f, -0.00001f);
    assert(tiny.x > 0 && tiny.y < 0);
    PangeaTouchStick reset = PangeaTouchStick_Reset(100, 200);
    near(reset.x, 0); near(reset.y, 0);
    near(reset.knobX, 100); near(reset.knobY, 200);
    PangeaTouchStick invalid = PangeaTouchStick_Update(100, 200, 25, 25, 500, 500);
    near(invalid.x, 0); near(invalid.y, 0);
    invalid = PangeaTouchStick_Update(100, 200, 100, 25, NAN, 500);
    near(invalid.x, 0); near(invalid.y, 0);
}

static void test_layout(float width, float height, int count, int insets)
{
    float left = insets ? width * 0.06f : 0;
    float right = insets ? width * 0.03f : 0;
    float top = insets ? height * 0.04f : 0;
    float bottom = insets ? height * 0.08f : 0;
    TouchLayout layout;
    TouchLayout_ComputeWithInsets(&layout, width, height, count, left, top, right, bottom);
    assert(layout.actionCount == count);
    TouchControlCircle controls[12];
    int n = 0;
    controls[n++] = layout.stick;
    for (int i = 0; i < count; i++) controls[n++] = layout.actions[i];
    for (int i = 0; i < 3; i++) controls[n++] = layout.utility[i];
    float tolerance = fmaxf(width, height) * 0.00001f;
    for (int i = 0; i < n; i++)
    {
        TouchControlCircle c = controls[i];
        assert(c.radius > 0);
        assert(c.x - c.radius >= left - tolerance);
        assert(c.y - c.radius >= top - tolerance);
        assert(c.x + c.radius <= width - right + tolerance);
        assert(c.y + c.radius <= height - bottom + tolerance);
        assert(TouchLayout_Contains(&c, c.x, c.y));
        assert(!TouchLayout_Contains(&c, c.x + c.radius * 1.01f, c.y));
        for (int j = 0; j < i; j++)
            assert(hypotf(c.x - controls[j].x, c.y - controls[j].y) > c.radius + controls[j].radius);
    }
}

int main(void)
{
    test_stick();
    const float widths[] = {1, 32, 240, 320, 640, 800, 1280, 1920, 2560, 10000};
    const float heights[] = {1, 32, 240, 360, 480, 600, 720, 1080, 1600, 10000};
    for (unsigned w = 0; w < sizeof widths / sizeof widths[0]; w++)
        for (unsigned h = 0; h < sizeof heights / sizeof heights[0]; h++)
            for (int count = 0; count <= 8; count++)
                for (int insets = 0; insets <= 1; insets++)
                    test_layout(widths[w], heights[h], count, insets);
    TouchLayout invalid;
    TouchLayout_Compute(&invalid, 0, 100, 8);
    assert(invalid.stick.radius == 0 && invalid.actionCount == 0);
    TouchLayout_ComputeWithInsets(&invalid, 100, 100, 8, 60, 0, 60, 0);
    assert(invalid.stick.radius == 0 && invalid.actionCount == 0);
    TouchLayout_Compute(&invalid, 800, 480, 100);
    assert(invalid.actionCount == 8);
    TouchLayout_Compute(&invalid, 800, 480, -1);
    assert(invalid.actionCount == 0);
    for (int i = 0; i < 8; i++) assert(invalid.actions[i].radius == 0);
    puts("Android touch geometry: 1800 layouts and analogue sweep passed");
    return 0;
}
