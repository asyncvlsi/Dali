/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
/**
 * @file
 * The timing diagnostics pane.
 *
 * Header-only and free of any placement dependency, so a test can build one,
 * hand it captured metadata, and assert on what it selected and displayed
 * without running a placement or opening the main window.
 */
#ifndef DALI_GUI_QT_TIMING_DIAGNOSTICS_PANEL_H_
#define DALI_GUI_QT_TIMING_DIAGNOSTICS_PANEL_H_

#include <QCheckBox>
#include <QColor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStyle>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTextDocument>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <string>
#include <vector>

#include "dali/common/placement_snapshot_sink.h"

namespace dali {

/**
 * The timing diagnostics pane: a tree of delay lines and an inspector.
 *
 * Everything shown here was measured by Dali and copied into the snapshot.
 * Selecting a row changes what is drawn and nothing else -- no timing is run,
 * no sizing is recomputed, and the numbers are displayed exactly as they were
 * captured, beside the name of the sample they came from.
 *
 * Selection follows semantic identity across samples, never the numeric timer
 * id. Ids are renumbered by an ACT re-elaboration while the constraint they
 * point at stays the same, so a viewer that remembered the number would quietly
 * switch to a different constraint at the one moment the user is watching for a
 * change.
 */
class TimingDiagnosticsPanel : public QWidget {
 public:
  /** Roles carrying the model's keys; the numeric id is never the key. */
  enum Roles {
    kConstraintIdRole = Qt::UserRole + 1,
    kSemanticIdentityRole,
    kDelayLineRole,
    kIsConstraintRole,
  };

  explicit TimingDiagnosticsPanel(QWidget* parent = nullptr) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    tree_ = new QTreeView(this);
    model_ = new QStandardItemModel(this);
    model_->setHorizontalHeaderLabels({"Delay line", "Slack"});
    tree_->setModel(model_);
    tree_->setUniformRowHeights(true);
    tree_->setAllColumnsShowFocus(true);
    tree_->header()->setStretchLastSection(true);
    inspector_ = new QLabel(this);
    inspector_->setTextFormat(Qt::RichText);
    inspector_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    inspector_->setWordWrap(true);
    inspector_->setToolTip(
        "Path delays are the sum of the captured witness steps. They reconcile "
        "with the timer's reported slack to about 6.3e-9 relative, the "
        "floating-point accumulation error of summing several hundred terms.");
    layout->addWidget(tree_, 3);
    layout->addWidget(inspector_, 2);

    // The path toggles belong to the pane whose selection they describe, not
    // to the global View strip: they do nothing at all on an ordinary
    // placement run, and a control that is inert most of the time reads as
    // broken rather than as inapplicable. The pane owns their state and
    // reports it as plain values; it never touches the canvas.
    // The delay-line layers belong beside the timing evidence too, but unlike
    // the path toggles they describe the design rather than a selection, so
    // they stay enabled whether or not a constraint is chosen.
    auto* layers = new QHBoxLayout();
    layers->setContentsMargins(0, 0, 0, 0);
    auto* layers_label = new QLabel("Layers:", this);
    QFont layers_font = layers_label->font();
    layers_font.setBold(true);
    layers_label->setFont(layers_font);
    layers->addWidget(layers_label);
    delay_line_checkbox_ = new QCheckBox("Delay lines", this);
    delay_line_checkbox_->setToolTip(
        "Delay-line cells: orange for members the run started with, dark "
        "orange for members inserted by the topology change. Inserted members "
        "keep the delay-line colour rather than the topology-addition cyan, so "
        "the line reads as one object.");
    topology_added_checkbox_ = new QCheckBox("Topology additions", this);
    topology_added_checkbox_->setToolTip(
        "Cyan for cells the topology change added that are not delay-line "
        "members. Well taps and end caps are added by physical completion "
        "instead, and keep their own colours.");
    layers->addWidget(delay_line_checkbox_);
    layers->addWidget(topology_added_checkbox_);
    layers->addStretch();
    layout->addLayout(layers);
    QObject::connect(delay_line_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_show_delay_lines) on_show_delay_lines(on); });
    QObject::connect(topology_added_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_show_topology_added) on_show_topology_added(on); });

    // Two rows of two, not one row of four: the pane is 330 px at its minimum
    // and a single row truncated every label to "Fad(", "Slov", "Sha(".
    auto* path_display = new QVBoxLayout();
    path_display->setContentsMargins(0, 0, 0, 0);
    auto* path_header = new QHBoxLayout();
    auto* path_label = new QLabel("Path display:", this);
    QFont path_font = path_label->font();
    path_font.setBold(true);
    path_label->setFont(path_font);
    path_header->addWidget(path_label);
    path_header->addStretch();
    fade_unrelated_checkbox_ = new QCheckBox("Fade unrelated", this);
    fast_path_checkbox_ = new QCheckBox("Fast path", this);
    slow_path_checkbox_ = new QCheckBox("Slow path", this);
    shared_path_checkbox_ = new QCheckBox("Shared path", this);
    fast_path_checkbox_->setChecked(true);
    slow_path_checkbox_->setChecked(true);
    shared_path_checkbox_->setChecked(true);
    fade_unrelated_checkbox_->setToolTip(
        "Subdue cells that are not on the selected constraint's witnesses.");
    clear_selection_button_ = new QPushButton(this);
    clear_selection_button_->setIcon(
        style()->standardIcon(QStyle::SP_DialogResetButton));
    clear_selection_button_->setToolTip("Clear the selected constraint");
    clear_selection_button_->setAccessibleName("Clear selection");
    path_header->addWidget(clear_selection_button_);
    path_display->addLayout(path_header);

    auto* path_boxes = new QGridLayout();
    path_boxes->setContentsMargins(0, 0, 0, 0);
    path_boxes->addWidget(fade_unrelated_checkbox_, 0, 0);
    path_boxes->addWidget(fast_path_checkbox_, 0, 1);
    path_boxes->addWidget(slow_path_checkbox_, 1, 0);
    path_boxes->addWidget(shared_path_checkbox_, 1, 1);
    path_display->addLayout(path_boxes);
    layout->addLayout(path_display);

    QObject::connect(fade_unrelated_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_fade_unrelated) on_fade_unrelated(on); });
    QObject::connect(fast_path_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_show_fast_path) on_show_fast_path(on); });
    QObject::connect(slow_path_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_show_slow_path) on_show_slow_path(on); });
    QObject::connect(shared_path_checkbox_, &QCheckBox::toggled, this,
                     [this](bool on) { if (on_show_shared_path) on_show_shared_path(on); });
    QObject::connect(clear_selection_button_, &QPushButton::clicked, this,
                     [this]() { ClearSelection(); });
    UpdatePathControlsEnabled();

    QObject::connect(tree_->selectionModel(),
                     &QItemSelectionModel::currentChanged, this,
                     [this](const QModelIndex& current, const QModelIndex&) {
                       OnRowSelected(current);
                     });
    setMinimumWidth(330);
  }

  bool HasTimingData() const { return !lines_.empty(); }

  /** Rebuild from a new sample, preserving the selection by identity. */
  void SetTimingData(
      const std::string& sample_stage,
      const std::vector<PlacementDelayLineTimingVisualization>& lines,
      const std::vector<PlacementTimingPathVisualization>& unattributed,
      const std::vector<PlacementTimingPathVisualization>& ambiguous,
      const std::vector<PlacementSizingDecisionEvidence>& decisions) {
    sample_stage_ = sample_stage;
    lines_ = lines;
    unattributed_ = unattributed;
    ambiguous_ = ambiguous;
    decisions_ = decisions;

    const std::string wanted_identity = selected_identity_;
    const std::string wanted_line = selected_line_;
    Rebuild();
    // Nothing is selected until a person selects it. Highlighting the globally
    // worst constraint on arrival made the viewer assert a conclusion the run
    // had not reached, and there is no way to tell that apart from a selection
    // the operator made. A selection already in hand is still followed by
    // identity, and falls back within its own line before it is given up.
    if (wanted_line.empty() || !RestoreSelection(wanted_line, wanted_identity)) {
      ClearSelection();
    }
  }

  const PlacementTimingPathVisualization* SelectedPath() const {
    return FindPath(selected_line_, selected_identity_);
  }
  std::string SelectedLine() const { return selected_line_; }
  std::string SelectedIdentity() const { return selected_identity_; }
  int SelectedConstraintId() const {
    const PlacementTimingPathVisualization* path = SelectedPath();
    return path == nullptr ? -1 : path->constraint_id;
  }
  QString InspectorText() const { return inspector_->text(); }

  /**
   * The inspector's text with the markup removed.
   *
   * Written beside a capture so a checker can verify what the pane said. A PNG
   * cannot be read without OCR, and the wording is exactly the thing most worth
   * checking -- the mixed-scope line this replaced was wrong in its words, not
   * in its pixels.
   */
  QString InspectorPlainText() const {
    QTextDocument document;
    document.setHtml(inspector_->text());
    return document.toPlainText();
  }
  QTreeView* Tree() { return tree_; }
  QStandardItemModel* Model() { return model_; }
  /** Set by the owner; called only when the drawn selection must change. */
  std::function<void(const PlacementTimingPathVisualization*)> on_selection;

  /** Select a constraint by identity, as a test or a click would. */
  bool SelectByIdentity(const std::string& line, const std::string& identity) {
    return RestoreSelection(line, identity);
  }

  /**
   * Select a line's worst constraint, as clicking that line's row does.
   *
   * Used by unattended capture so a frame can be requested for a named line
   * without a person clicking it. It goes through the same Select() the click
   * handler uses, so a capture cannot reach a selection state a user cannot.
   */
  bool SelectLineWorst(const std::string& line) {
    for (const PlacementDelayLineTimingVisualization& entry : lines_) {
      if (entry.delay_line_name != line || entry.constraints.empty()) continue;
      Select(line, entry.constraints.front().semantic_identity);
      return true;
    }
    return false;
  }

  /** Select one numeric constraint under a line, as clicking its row does. */
  bool SelectConstraintId(const std::string& line, int constraint_id) {
    for (const PlacementDelayLineTimingVisualization& entry : lines_) {
      if (entry.delay_line_name != line) continue;
      for (const PlacementTimingPathVisualization& path : entry.constraints) {
        if (path.constraint_id != constraint_id) continue;
        Select(line, path.semantic_identity);
        return true;
      }
    }
    return false;
  }

 private:
  static QString FormatPs(double value) {
    return QString("%1%2 ps")
        .arg(value >= 0 ? "+" : "")
        .arg(value, 0, 'f', 3);
  }

  void Rebuild() {
    model_->removeRows(0, model_->rowCount());
    for (const PlacementDelayLineTimingVisualization& line : lines_) {
      AppendGroup(QString::fromStdString(line.delay_line_name), line.constraints,
                  line.delay_line_name, !line.constraints.empty(),
                  line.worst_slack_ps);
    }
    // Only when nonempty: an always-present empty group is a permanent
    // suggestion that something is wrong.
    if (!unattributed_.empty()) {
      AppendGroup("Unattributed", unattributed_, "", true,
                  unattributed_.front().slack_ps);
    }
    if (!ambiguous_.empty()) {
      AppendGroup("Ambiguous", ambiguous_, "", true,
                  ambiguous_.front().slack_ps);
    }
    tree_->expandAll();
    tree_->resizeColumnToContents(0);
  }

  void AppendGroup(const QString& title,
                   const std::vector<PlacementTimingPathVisualization>& paths,
                   const std::string& line_key, bool has_worst,
                   double worst_slack) {
    auto* name = new QStandardItem(title);
    name->setEditable(false);
    name->setData(QString::fromStdString(line_key), kDelayLineRole);
    name->setData(false, kIsConstraintRole);
    auto* slack = new QStandardItem(has_worst ? FormatPs(worst_slack) : "-");
    slack->setEditable(false);
    for (std::size_t index = 0; index < paths.size(); ++index) {
      const PlacementTimingPathVisualization& path = paths[index];
      auto* label = new QStandardItem(
          index == 0 ? QString("Worst constraint   #%1").arg(path.constraint_id)
                     : QString("Constraint         #%1").arg(path.constraint_id));
      label->setEditable(false);
      // The numeric id is a diagnostic handle, so it is shown muted; the
      // identity is the key and lives in the model rather than the label.
      QFont font = label->font();
      font.setPointSizeF(font.pointSizeF() - 0.5);
      label->setFont(font);
      label->setForeground(QColor(71, 85, 105));
      label->setData(path.constraint_id, kConstraintIdRole);
      label->setData(QString::fromStdString(path.semantic_identity),
                     kSemanticIdentityRole);
      label->setData(QString::fromStdString(line_key), kDelayLineRole);
      label->setData(true, kIsConstraintRole);
      label->setToolTip(QString::fromStdString(path.semantic_identity));
      auto* child_slack = new QStandardItem(FormatPs(path.slack_ps));
      child_slack->setEditable(false);
      name->appendRow({label, child_slack});
    }
    model_->appendRow({name, slack});
  }

  const PlacementTimingPathVisualization* FindPath(
      const std::string& line, const std::string& identity) const {
    if (identity.empty()) return nullptr;
    auto search = [&identity](
        const std::vector<PlacementTimingPathVisualization>& paths)
        -> const PlacementTimingPathVisualization* {
      for (const PlacementTimingPathVisualization& path : paths) {
        if (path.semantic_identity == identity) return &path;
      }
      return nullptr;
    };
    for (const PlacementDelayLineTimingVisualization& entry : lines_) {
      if (!line.empty() && entry.delay_line_name != line) continue;
      if (const auto* found = search(entry.constraints)) return found;
    }
    if (const auto* found = search(unattributed_)) return found;
    if (const auto* found = search(ambiguous_)) return found;
    return nullptr;
  }

  /** The line whose worst constraint is worst overall. */
 public:
  /**
   * Drop the selection entirely: no row, no highlight, no path controls.
   *
   * Sends a null selection outward so the canvas clears its highlight through
   * the same channel a click uses, rather than the window reaching in.
   */
  void ClearSelection() {
    // Only report a change that happened. An ordinary placement run publishes
    // no timing data and must stay silent; notifying that nothing became
    // nothing would make the pane chatter at every snapshot.
    const bool had_selection = !selected_line_.empty() || !selected_identity_.empty();
    selected_line_.clear();
    selected_identity_.clear();
    if (tree_->selectionModel() != nullptr) {
      const QSignalBlocker blocker(tree_->selectionModel());
      tree_->selectionModel()->clearSelection();
      tree_->setCurrentIndex(QModelIndex());
    }
    UpdateInspector();
    UpdatePathControlsEnabled();
    if (had_selection && on_selection) on_selection(nullptr);
  }

  QCheckBox* FadeUnrelatedCheckbox() { return fade_unrelated_checkbox_; }
  QCheckBox* DelayLineCheckbox() { return delay_line_checkbox_; }
  QCheckBox* TopologyAddedCheckbox() { return topology_added_checkbox_; }
  QCheckBox* FastPathCheckbox() { return fast_path_checkbox_; }
  QCheckBox* SlowPathCheckbox() { return slow_path_checkbox_; }
  QCheckBox* SharedPathCheckbox() { return shared_path_checkbox_; }
  QPushButton* ClearSelectionButton() { return clear_selection_button_; }

  /** True when the path toggles currently act on something. */
  bool PathControlsEnabled() const {
    return fade_unrelated_checkbox_ != nullptr &&
           fade_unrelated_checkbox_->isEnabled();
  }

  /** Value-only outward reports; the pane never touches the canvas. */
  std::function<void(bool)> on_show_delay_lines;
  std::function<void(bool)> on_show_topology_added;
  std::function<void(bool)> on_fade_unrelated;
  std::function<void(bool)> on_show_fast_path;
  std::function<void(bool)> on_show_slow_path;
  std::function<void(bool)> on_show_shared_path;

 private:
  /** The toggles describe a selection, so they are dead without one. */
  void UpdatePathControlsEnabled() {
    const bool enabled = SelectedPath() != nullptr;
    for (QCheckBox* box : {fade_unrelated_checkbox_, fast_path_checkbox_,
                           slow_path_checkbox_, shared_path_checkbox_}) {
      if (box != nullptr) box->setEnabled(enabled);
    }
    if (clear_selection_button_ != nullptr) {
      clear_selection_button_->setEnabled(enabled);
    }
  }

 public:
  void SelectDefault() {
    const PlacementDelayLineTimingVisualization* worst = nullptr;
    for (const PlacementDelayLineTimingVisualization& line : lines_) {
      if (line.constraints.empty()) continue;
      if (worst == nullptr || line.worst_slack_ps < worst->worst_slack_ps) {
        worst = &line;
      }
    }
    if (worst == nullptr) {
      selected_line_.clear();
      selected_identity_.clear();
      UpdateInspector();
      return;
    }
    Select(worst->delay_line_name, worst->constraints.front().semantic_identity);
  }

  bool RestoreSelection(const std::string& line, const std::string& identity) {
    if (FindPath(line, identity) != nullptr) {
      Select(line, identity);
      return true;
    }
    // The identity is gone. Fall back to this line's current worst constraint
    // rather than to whatever now carries the old number.
    for (const PlacementDelayLineTimingVisualization& entry : lines_) {
      if (entry.delay_line_name != line || entry.constraints.empty()) continue;
      Select(line, entry.constraints.front().semantic_identity);
      return true;
    }
    return false;
  }

  void Select(const std::string& line, const std::string& identity) {
    selected_line_ = line;
    selected_identity_ = identity;
    SyncTreeSelection();
    UpdateInspector();
    UpdatePathControlsEnabled();
    if (on_selection) on_selection(SelectedPath());
  }

  void SyncTreeSelection() {
    for (int group = 0; group < model_->rowCount(); ++group) {
      QStandardItem* parent = model_->item(group, 0);
      for (int child = 0; child < parent->rowCount(); ++child) {
        QStandardItem* item = parent->child(child, 0);
        if (item->data(kSemanticIdentityRole).toString().toStdString() ==
            selected_identity_) {
          const QSignalBlocker blocker(tree_->selectionModel());
          tree_->setCurrentIndex(item->index());
          // Bring it into view: a tree scrolled elsewhere shows a selection the
          // reader cannot see, which in a captured frame is indistinguishable
          // from no selection at all.
          tree_->scrollTo(item->index(), QAbstractItemView::PositionAtCenter);
          return;
        }
      }
    }
  }

  void OnRowSelected(const QModelIndex& index) {
    if (!index.isValid()) return;
    const QModelIndex first = index.sibling(index.row(), 0);
    if (first.data(kIsConstraintRole).toBool()) {
      Select(first.data(kDelayLineRole).toString().toStdString(),
             first.data(kSemanticIdentityRole).toString().toStdString());
      return;
    }
    // A delay-line row selects that line's worst constraint, which is the
    // question a user clicking a line is asking.
    const std::string line = first.data(kDelayLineRole).toString().toStdString();
    for (const PlacementDelayLineTimingVisualization& entry : lines_) {
      if (entry.delay_line_name != line || entry.constraints.empty()) continue;
      Select(line, entry.constraints.front().semantic_identity);
      return;
    }
  }

  const PlacementSizingDecisionEvidence* DecisionFor(
      const std::string& site) const {
    for (const PlacementSizingDecisionEvidence& entry : decisions_) {
      if (entry.site == site) return &entry;
    }
    return nullptr;
  }

  void UpdateInspector() {
    const PlacementTimingPathVisualization* path = SelectedPath();
    if (path == nullptr) {
      inspector_->setText("<b>No constraint selected</b>");
      return;
    }
    const QString sample = sample_stage_.empty()
                               ? "Unavailable"
                               : QString::fromStdString(sample_stage_);
    QString text;
    text += QString("<b>%1</b><br/>")
                .arg(path->attributed_delay_line.empty()
                         ? (path->ambiguous_attribution ? "Ambiguous"
                                                        : "Unattributed")
                         : QString::fromStdString(path->attributed_delay_line));
    const bool is_worst = IsWorst(*path);
    text += QString("%1 <span style='color:#475569'>#%2</span><br/><br/>")
                .arg(is_worst ? "Worst constraint" : "Constraint")
                .arg(path->constraint_id);

    text += "<b>Timing sample</b><br/>";
    text += Row("sample", sample);
    text += Row("fast path delay", QString("%1 ps").arg(path->fast_delay_ps, 0,
                                                        'f', 2));
    text += Row("slow path delay", QString("%1 ps").arg(path->slow_delay_ps, 0,
                                                        'f', 2));
    text += Row("slack", FormatPs(path->slack_ps));

    text += "<br/><b>Sizing decision</b><br/>";
    const PlacementSizingDecisionEvidence* decision =
        path->attributed_delay_line.empty()
            ? nullptr
            : DecisionFor(path->attributed_delay_line);
    if (decision == nullptr) {
      text += Row("decision point", "Unavailable");
    } else {
      text += Row("decision point",
                  QString::fromStdString(decision->decision_point));
      text += Row("boundary slack", decision->has_boundary_slack
                                        ? FormatPs(decision->boundary_slack_ps)
                                        : "Unavailable");
      text += Row("pair count", QString("%1 -> %2")
                                    .arg(decision->current_pairs)
                                    .arg(decision->requested_pairs));
      text += Row("response used", decision->has_measured_response
                                       ? FormatPs(decision->measured_response_ps)
                                       : "Unavailable");
      // Two scopes, two blocks. Site-local additions and batch-global rewires
      // never share a line: every rewired net joins two neighbouring sites, so
      // no rewire belongs to one of them, and "+6 cells / +6 nets, 8 rewired"
      // reads as though eight were this site's.
      text += "<br/><b>Site request</b><br/>";
      text += Row("expected growth",
                  QString("+%1 cells / +%2 nets")
                      .arg(decision->expected_added_components)
                      .arg(decision->expected_added_nets));
      text += Row("applied growth",
                  decision->actual_added_components < 0
                      ? "Unavailable"
                      : QString("+%1 cells / +%2 nets")
                            .arg(decision->actual_added_components)
                            .arg(decision->actual_added_nets));
      text += "<br/><b>Batch application</b><br/>";
      text += Row("actual topology",
                  decision->batch_added_components < 0
                      ? "Unavailable"
                      : QString("+%1 cells / +%2 nets")
                            .arg(decision->batch_added_components)
                            .arg(decision->batch_added_nets));
      text += Row("retired / rewired",
                  decision->batch_rewired_nets < 0
                      ? "Unavailable"
                      : QString("%1 / %2 nets")
                            .arg(decision->batch_retired_nets)
                            .arg(decision->batch_rewired_nets));
      text += Row("final result",
                  decision->has_final_slack
                      ? QString("%1, %2")
                            .arg(FormatPs(decision->final_slack_ps))
                            .arg(decision->closed ? "closed" : "short")
                      : "Unavailable");
    }
    inspector_->setText(text);
  }

  bool IsWorst(const PlacementTimingPathVisualization& path) const {
    for (const PlacementDelayLineTimingVisualization& line : lines_) {
      if (line.delay_line_name != path.attributed_delay_line) continue;
      return !line.constraints.empty() &&
             line.constraints.front().semantic_identity ==
                 path.semantic_identity;
    }
    return false;
  }

  static QString Row(const QString& name, const QString& value) {
    return QString("&nbsp;&nbsp;%1<span style='color:#475569'> &nbsp; %2</span>"
                   "<br/>")
        .arg(name, value);
  }

  QCheckBox* delay_line_checkbox_ = nullptr;
  QCheckBox* topology_added_checkbox_ = nullptr;
  QCheckBox* fade_unrelated_checkbox_ = nullptr;
  QCheckBox* fast_path_checkbox_ = nullptr;
  QCheckBox* slow_path_checkbox_ = nullptr;
  QCheckBox* shared_path_checkbox_ = nullptr;
  QPushButton* clear_selection_button_ = nullptr;
  QTreeView* tree_ = nullptr;
  QStandardItemModel* model_ = nullptr;
  QLabel* inspector_ = nullptr;
  std::string sample_stage_;
  std::vector<PlacementDelayLineTimingVisualization> lines_;
  std::vector<PlacementTimingPathVisualization> unattributed_;
  std::vector<PlacementTimingPathVisualization> ambiguous_;
  std::vector<PlacementSizingDecisionEvidence> decisions_;
  std::string selected_line_;
  std::string selected_identity_;
};

}  // namespace dali

#endif  // DALI_GUI_QT_TIMING_DIAGNOSTICS_PANEL_H_
