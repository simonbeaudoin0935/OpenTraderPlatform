#pragma once

#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVector>
#include <expected>
#include <optional>

#include "ExternalStrategyDescription.h"
#include "StrategyConfig.h"

/**
 * @brief Dialog for loading a strategy runtime
 *
 * Lets the user browse for an external strategy executable, introspect its
 * self-described parameter schema, and build a StrategyConfig from the returned
 * metadata plus user-entered field values.
 */
class StrategyLoadDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit StrategyLoadDialog(QWidget* parent = nullptr);
    StrategyLoadDialog(const StrategyConfig& p_existingConfig, QWidget* parent = nullptr);
    ~StrategyLoadDialog() override = default;

    /// @brief Get the configured strategy config if dialog was accepted
    [[nodiscard]] std::optional<StrategyConfig> getSelectedConfig() const;

  private slots:
    void onBrowseClicked();
    void onLoadClicked();
    void onCancelClicked();

  private:
    void setupUI();
    void validateForm();
    void loadSelection(const QString& p_selectedPath, const std::map<QString, QJsonValue>& p_initialValues = {});
    void applyDescription(const ExternalStrategyDescriptionData& p_description,
                          const std::map<QString, QJsonValue>& p_initialValues = {});

    /// @brief Rebuild the discovered field section from a parameter schema.
    void populateFields(const QVector<ExternalStrategyFieldDefinition>& p_schema,
                        const std::map<QString, QJsonValue>& p_initialValues = {});

    /// @brief Remove all dynamically-added field rows.
    void clearFields();

    [[nodiscard]] std::expected<std::map<QString, QJsonValue>, QString> collectFieldValues() const;

    QLineEdit* m_runtimePathEdit;
    QPushButton* m_browseButton;
    QLabel* m_descriptionLabel;
    QPushButton* m_loadButton;
    QPushButton* m_cancelButton;

    QScrollArea* m_fieldsScrollArea = nullptr;
    QGroupBox* m_fieldsGroup = nullptr;
    QFormLayout* m_fieldsFormLayout = nullptr;

    struct FieldWidget
    {
        ExternalStrategyFieldDefinition definition;
        QWidget* widget = nullptr;
    };
    QVector<FieldWidget> m_fieldWidgets;
    std::optional<ExternalStrategyDescriptionData> m_description;
    bool m_editMode = false;

    std::optional<StrategyConfig> m_selectedConfig;
};
