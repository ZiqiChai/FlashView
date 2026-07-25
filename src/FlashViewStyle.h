#ifndef FLASHVIEWSTYLE_H
#define FLASHVIEWSTYLE_H

#include <QProxyStyle>

class FlashViewStyle final : public QProxyStyle
{
public:
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override
    {
        // Menu actions use PM_SmallIconSize. Eighteen pixels balances the
        // icons against FlashView's menu text without dominating the row.
        if (metric == PM_SmallIconSize)
            return 18;
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

#endif // FLASHVIEWSTYLE_H
