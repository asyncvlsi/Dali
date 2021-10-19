#include "blocktypewell.h"

namespace dali {

void BlockTypeWell::setNWellRect(int lx, int ly, int ux, int uy) {
    is_n_set_ = true;
    n_rect_.SetValue(lx, ly, ux, uy);
    if (is_p_set_) {
        DaliExpects(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
        p_n_edge_ = n_rect_.LLY();
    }
}

void BlockTypeWell::setPWellRect(int lx, int ly, int ux, int uy) {
    is_p_set_ = true;
    p_rect_.SetValue(lx, ly, ux, uy);
    if (is_n_set_) {
        DaliExpects(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
        p_n_edge_ = p_rect_.URY();
    }
}

void BlockTypeWell::setWellRect(bool is_n, int lx, int ly, int ux, int uy) {
    if (is_n) {
        setNWellRect(lx, ly, ux, uy);
    } else {
        setPWellRect(lx, ly, ux, uy);
    }
}

void BlockTypeWell::SetWellShape(bool is_n, RectI &rect) {
    setWellRect(is_n, rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}

bool BlockTypeWell::IsNPWellAbutted() const {
    if (is_p_set_ && is_n_set_) {
        return p_rect_.URY() == n_rect_.LLY();
    }
    return true;
}

void BlockTypeWell::Report() const {
    BOOST_LOG_TRIVIAL(info)
        << "  Well of BlockType: " << *(type_ptr_->NamePtr()) << "\n"
        << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  "
        << n_rect_.URX() << "  " << n_rect_.URY()
        << "\n"
        << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  "
        << p_rect_.URX() << "  " << p_rect_.URY()
        << "\n";
}

}

