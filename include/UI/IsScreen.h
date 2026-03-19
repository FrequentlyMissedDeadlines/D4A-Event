#pragma once

class IsScreen
{
public:
    /**
     * @brief Initializes the screen. This method can be overridden by derived classes to set up the screen's components and layout.
     */
    virtual void initScreen() {}

    /**
     * @brief Updates the screen. This method can be overridden by derived classes to update the screen's content.
     */
    virtual void updateScreen() {}

    /**
     * @brief Returns true if the screen content has changed since the last draw.
     *
     * Override in derived classes to skip redraws when nothing changed.
     * The default always returns true (redraw every tick).
     */
    virtual bool needsUpdate() const { return true; }

    virtual ~IsScreen() = default;
};