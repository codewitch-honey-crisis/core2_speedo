#pragma once
#include <uix_core.hpp>
/// @brief Represents an analog clock
/// @tparam ControlSurfaceType The control surface type, usually taken from the screen
template <typename ControlSurfaceType>
class svg_needle : public uix::canvas_control<ControlSurfaceType> {
   public:
    /// @brief The type of the instance
    using type = svg_needle;
    /// @brief The pixel type of the draw surface
    using pixel_type = typename ControlSurfaceType::pixel_type;
    /// @brief The palette type of the draw surface
    using palette_type = typename ControlSurfaceType::palette_type;

   private:
    using base_type = uix::canvas_control<ControlSurfaceType>;
    using control_surface_type = ControlSurfaceType;
    
    int m_angle;
    bool m_dirty;
    gfx::vector_pixel m_needle_color, m_needle_border_color;
    uint16_t m_needle_border_width;
    void do_copy_fields(const svg_needle& rhs) {
        m_angle = rhs.m_angle;
        m_needle_color = rhs.m_needle_color;
        m_needle_border_color = rhs.m_needle_border_color;
        m_needle_border_width = rhs.m_needle_border_width;
    }
    void update_transform(float rotation, float& ctheta, float& stheta) {
        float rads = rotation * (3.1415926536f / 180.0f);
        ctheta = cosf(rads);
        stheta = sinf(rads);
    }
    constexpr gfx::pointf translate(float ctheta, float stheta, gfx::pointf center, gfx::pointf offset, float x, float y) const {
        float rx = (ctheta * (x - (float)center.x) - stheta * (y - (float)center.y) + (float)center.x) + offset.x;
        float ry = (stheta * (x - (float)center.x) + ctheta * (y - (float)center.y) + (float)center.y) + offset.y;
        return {(float)rx, (float)ry};
    }

   protected:
    /// @brief Moves the control
    /// @param rhs The control to move from
    void do_move_control(svg_needle& rhs) {
        this->base_type::do_move_control(rhs);
        do_copy_fields(rhs);
        m_dirty = true;
    }
    /// @brief Copies the control
    /// @param rhs The control to copy from
    void do_copy_control(const svg_needle& rhs) {
        this->base_type::do_copy_control(rhs);
        do_copy_fields(rhs);
        m_dirty = true;
    }

   public:
    /// @brief Moves a new clock control from an existing one
    /// @param rhs The existing clock
    svg_needle(svg_needle&& rhs) {
        do_move_control(rhs);
    }
    /// @brief Moves the clock control from an existing one
    /// @param rhs The existing clock
    /// @return This clock
    svg_needle& operator=(svg_needle&& rhs) {
        do_move_control(rhs);
        return *this;
    }
    /// @brief Copies a new clock control from an existing one
    /// @param rhs The existing clock
    svg_needle(const svg_needle& rhs) {
        do_copy_control(rhs);
    }
    /// @brief Copies the clock control from an existing one
    /// @param rhs The exisitng clock
    /// @return This clock
    svg_needle& operator=(const svg_needle& rhs) {
        do_copy_control(rhs);
        return *this;
    }
    /// @brief Constructs a new instance of a needle
    /// @param parent The parent (a screen)
    /// @param palette The palette, if applicable
    svg_needle(uix::invalidation_tracker& parent, const palette_type* palette = nullptr)
        : base_type(parent, palette), m_angle(0), m_dirty(true) {
        static const constexpr gfx::vector_pixel white(0xFF,0xFF, 0xFF, 0xFF);
        static const constexpr gfx::vector_pixel black(0xFF,0x0, 0x0, 0x0);
        static const constexpr gfx::vector_pixel gray(0xFF,0x7F, 0x7F, 0x7F);
        static const constexpr gfx::vector_pixel red(0xFF,0xFF, 0x0, 0x0);
        
        m_needle_color = red;
        m_needle_border_color = red;
        m_needle_border_width = 1;
    }
    /// @brief Constructs a new instance of a needle
    svg_needle()
        : base_type(), m_angle(0), m_dirty(true) {
        static const constexpr gfx::vector_pixel white(0xFF,0xFF, 0xFF, 0xFF);
        static const constexpr gfx::vector_pixel black(0xFF,0x0, 0x0, 0x0);
        static const constexpr gfx::vector_pixel gray(0xFF,0x7F, 0x7F, 0x7F);
        static const constexpr gfx::vector_pixel red(0xFF,0xFF, 0x0, 0x0);    
        m_needle_color = red;
        m_needle_border_color = red;
        m_needle_border_width = 1;
    }
    /// @brief Indicates the angle in degrees
    /// @return The value
    int angle() const {
        return m_angle;
    }
    /// @brief Sets the angle in degrees
    /// @param value The value
    void angle(int value) {
        if (value != m_angle) {
            m_angle = value % 360;
            m_dirty = true;
            this->invalidate();
        }
    }
    
    /// @brief Indicates the color of the needle
    /// @return The color
    gfx::rgba_pixel<32> needle_color() const {
        gfx::rgba_pixel<32> result;
        gfx::convert(m_needle_color,&result);
        return result;
    }
    /// @brief Sets the color of the needle
    /// @param value The color
    void needle_color(gfx::rgba_pixel<32> value) {
        gfx::convert(value,&m_needle_color);
        m_dirty = true;
        this->invalidate();
    }
    /// @brief Indicates the color of the needle border
    /// @return The color
    gfx::rgba_pixel<32> needle_border_color() const {
        gfx::rgba_pixel<32> result;
        gfx::convert(m_needle_border_color,&result);
        return result;
    }
    /// @brief Sets the color of the needle border
    /// @param value The color
    void needle_border_color(gfx::rgba_pixel<32> value) {
        gfx::convert(value,&m_needle_border_color);
        m_dirty = true;
        this->invalidate();
    }
    /// @brief Indicates the border width of the needle
    /// @return The width in pixels
    uint16_t needle_border_width() const {
        return m_needle_border_width;
    }
    /// @brief Sets the border width of the needle
    /// @param value The width in pixels
    void needle_border_width(uint16_t value) {
        m_needle_border_width = value;
        m_dirty = true;
        this->invalidate();
    }
   protected:
    /// @brief Paints the control
    /// @param destination The surface to draw to
    /// @param clip The clipping rectangle
    virtual void on_paint(gfx::canvas& destination, const uix::srect16& clip) override {
        // get the rect for the drawing area
        gfx::canvas_style si = destination.style();
        si.fill_paint_type = gfx::paint_type::solid;
        si.stroke_paint_type = gfx::paint_type::solid;
        gfx::pointf offset(0, 0);
        gfx::pointf center(0, 0);
        float rotation(0);
        float ctheta, stheta;
        gfx::ssize16 size = this->bounds().dimensions();
        gfx::rectf b = gfx::sizef(size.width, size.height).bounds();
        float w = b.width();
        float h = b.height();
        if(w>h) w= h;
        center = gfx::pointf(w * 0.5f + 1, w * 0.5f + 1);
        gfx::rectf sr = gfx::rectf(0, w / 40, w / 16, w / 2);
        sr.center_horizontal_inplace(b);
        rotation = m_angle;
        update_transform(rotation, ctheta, stheta);
        destination.move_to(translate(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y1));
        destination.line_to(translate(ctheta, stheta, center, offset, sr.x2, sr.y2));
        destination.line_to(translate(ctheta, stheta, center, offset, sr.x1 + sr.width() * 0.5f, sr.y2 + (w / 20)));
        destination.line_to(translate(ctheta, stheta, center, offset, sr.x1, sr.y2));
        si.fill_color = m_needle_color;
        si.stroke_color = m_needle_border_color;
        si.stroke_width = m_needle_border_width;
        destination.style(si);
        destination.render();
    
    }
    
};
