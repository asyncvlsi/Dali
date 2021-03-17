//
// Created by yihan on 7/13/19.
//

#include "freesegment.h"

namespace dali {

FreeSegment::FreeSegment(int start, int stop) :
    start_(start),
    end_(stop) {
  assert(start <= stop);
}

bool FreeSegment::LinkSingleSeg(FreeSegment *seg_ptr) {
  if (seg_ptr == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "Want to link to an Empty pointer?\n";
    assert(seg_ptr != nullptr);
  }
  if ((this->Next() != nullptr) || (seg_ptr->Next() != nullptr)) {
    BOOST_LOG_TRIVIAL(info) << "This member function is not for concatenating multi nodes linked list\n";
    assert((this->Next() == nullptr) && (seg_ptr->Next() == nullptr));
  }
  return (SetNext(seg_ptr) && seg_ptr->SetPrev(this));
}

FreeSegment *FreeSegment::SingleSegOr(FreeSegment *seg) {
  if ((Length() == 0) && (seg->Length() == 0)) {
    BOOST_LOG_TRIVIAL(info) << "What?! two segments with Length 0 for OR operation\n";
    return nullptr;
  }
  auto *result = new FreeSegment;
  if (start_ > seg->End() || end_ < seg->Start()) { // no overlap, no touch
    int firstStart, firstEnd, secondStart, secondEnd;
    if (start_ < seg->Start()) {
      firstStart = start_;
      firstEnd = end_;
      secondStart = seg->Start();
      secondEnd = seg->End();
    } else {
      firstStart = seg->Start();
      firstEnd = seg->End();
      secondStart = start_;
      secondEnd = end_;
    }
    result->SetSpan(firstStart, firstEnd);
    auto *secondSeg = new FreeSegment(secondStart, secondEnd);
    result->SetNext(secondSeg);
  } else if (start_ == seg->End()) { // start_ touches the End of seg
    result->SetSpan(seg->Start(), end_);
  } else if (end_ == seg->Start()) { // end_ touches the Start of seg
    result->SetSpan(start_, seg->End());
  } else { // non-zero overlap
    result->SetSpan(std::min(start_, seg->Start()), std::max(end_, seg->End()));
  }
  return result;
}

void FreeSegment::Clear() {
  FreeSegment *current = this;
  FreeSegment *next = nullptr;
  while (current != nullptr) {
    next = current->Next();
    delete current;
    current = next;
  }
}

}
