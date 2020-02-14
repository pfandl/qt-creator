#include <vcsbase/vcscommand.h>
using namespace std::placeholders;
class MercurialTopicCache : public Core::IVersionControl::TopicCache
public:
    MercurialTopicCache(MercurialClient *client) : m_client(client) {}

protected:
    QString trackFile(const QString &repository) override
    {
        return repository + QLatin1String("/.hg/branch");
    }

    QString refreshTopic(const QString &repository) override
    {
        return m_client->branchQuerySync(repository);
    }

private:
    MercurialClient *m_client;
};

const VcsBaseEditorParameters logEditorParameters {
    Constants::LOGAPP
};
const VcsBaseEditorParameters annotateEditorParameters {
    AnnotateOutput,
    Constants::ANNOTATEAPP
};
const VcsBaseEditorParameters diffEditorParameters {
    DiffOutput,
    Constants::DIFFAPP
const VcsBaseSubmitEditorParameters submitEditorParameters {
    // IVersionControl
    QString displayName() const final;
    Core::Id id() const final;
    bool isVcsFileOrDirectory(const Utils::FilePath &fileName) const final;

    bool managesDirectory(const QString &filename, QString *topLevel = nullptr) const final;
    bool managesFile(const QString &workingDirectory, const QString &fileName) const final;
    bool isConfigured() const final;
    bool supportsOperation(Operation operation) const final;
    bool vcsOpen(const QString &fileName) final;
    bool vcsAdd(const QString &filename) final;
    bool vcsDelete(const QString &filename) final;
    bool vcsMove(const QString &from, const QString &to) final;
    bool vcsCreateRepository(const QString &directory) final;
    bool vcsAnnotate(const QString &file, int line) final;

    Core::ShellCommand *createInitialCheckoutCommand(const QString &url,
                                                     const Utils::FilePath &baseDirectory,
                                                     const QString &localName,
                                                     const QStringList &extraArgs) final;

    bool sccManaged(const QString &filename);

    // To be connected to the HgTask's success signal to emit the repository/
    // files changed signals according to the variant's type:
    // String -> repository, StringList -> files
    void changed(const QVariant&);

    void describe(const QString &source, const QString &id) { m_client.view(source, id); }

    OptionsPage m_optionsPage{[this] { configurationChanged(); }, &m_settings};

public:
    VcsSubmitEditorFactory submitEditorFactory {
        submitEditorParameters,
        [] { return new CommitEditor; },
        this
    };

    VcsEditorFactory logEditorFactory {
        &logEditorParameters,
        [this] { return new MercurialEditorWidget(&m_client); },
        std::bind(&MercurialPluginPrivate::describe, this, _1, _2)
    };

    VcsEditorFactory annotateEditorFactory {
        &annotateEditorParameters,
        [this] { return new MercurialEditorWidget(&m_client); },
        std::bind(&MercurialPluginPrivate::describe, this, _1, _2)
    };

    VcsEditorFactory diffEditorFactory {
        &diffEditorParameters,
        [this] { return new MercurialEditorWidget(&m_client); },
        std::bind(&MercurialPluginPrivate::describe, this, _1, _2)
    };
    : VcsBase::VcsBasePluginPrivate(Core::Context(Constants::MERCURIAL_CONTEXT))
    setTopicCache(new MercurialTopicCache(&m_client));
    Core::Context context(Constants::MERCURIAL_CONTEXT);
    connect(&m_client, &VcsBaseClient::changed, this, &MercurialPluginPrivate::changed);
    connect(&m_client, &MercurialClient::needUpdate, this, &MercurialPluginPrivate::update);
    const QString branch = vcsTopic(m_submitRepository);
QString MercurialPluginPrivate::displayName() const
{
    return tr("Mercurial");
}

Core::Id MercurialPluginPrivate::id() const
{
    return {VcsBase::Constants::VCS_ID_MERCURIAL};
}

bool MercurialPluginPrivate::isVcsFileOrDirectory(const Utils::FilePath &fileName) const
{
    return m_client.isVcsDirectory(fileName);
}

bool MercurialPluginPrivate::managesDirectory(const QString &directory, QString *topLevel) const
{
    QFileInfo dir(directory);
    const QString topLevelFound = m_client.findTopLevelForFile(dir);
    if (topLevel)
        *topLevel = topLevelFound;
    return !topLevelFound.isEmpty();
}

bool MercurialPluginPrivate::managesFile(const QString &workingDirectory, const QString &fileName) const
{
    return m_client.managesFile(workingDirectory, fileName);
}

bool MercurialPluginPrivate::isConfigured() const
{
    const Utils::FilePath binary = m_settings.binaryPath();
    if (binary.isEmpty())
        return false;
    QFileInfo fi = binary.toFileInfo();
    return fi.exists() && fi.isFile() && fi.isExecutable();
}

bool MercurialPluginPrivate::supportsOperation(Operation operation) const
{
    bool supported = isConfigured();
    switch (operation) {
    case Core::IVersionControl::AddOperation:
    case Core::IVersionControl::DeleteOperation:
    case Core::IVersionControl::MoveOperation:
    case Core::IVersionControl::CreateRepositoryOperation:
    case Core::IVersionControl::AnnotateOperation:
    case Core::IVersionControl::InitialCheckoutOperation:
        break;
    case Core::IVersionControl::SnapshotOperations:
        supported = false;
        break;
    }
    return supported;
}

bool MercurialPluginPrivate::vcsOpen(const QString &filename)
{
    Q_UNUSED(filename)
    return true;
}

bool MercurialPluginPrivate::vcsAdd(const QString &filename)
{
    const QFileInfo fi(filename);
    return m_client.synchronousAdd(fi.absolutePath(), fi.fileName());
}

bool MercurialPluginPrivate::vcsDelete(const QString &filename)
{
    const QFileInfo fi(filename);
    return m_client.synchronousRemove(fi.absolutePath(), fi.fileName());
}

bool MercurialPluginPrivate::vcsMove(const QString &from, const QString &to)
{
    const QFileInfo fromInfo(from);
    const QFileInfo toInfo(to);
    return m_client.synchronousMove(fromInfo.absolutePath(),
                                            fromInfo.absoluteFilePath(),
                                            toInfo.absoluteFilePath());
}

bool MercurialPluginPrivate::vcsCreateRepository(const QString &directory)
{
    return m_client.synchronousCreateRepository(directory);
}

bool MercurialPluginPrivate::vcsAnnotate(const QString &file, int line)
{
    const QFileInfo fi(file);
    m_client.annotate(fi.absolutePath(), fi.fileName(), QString(), line);
    return true;
}

Core::ShellCommand *MercurialPluginPrivate::createInitialCheckoutCommand(const QString &url,
                                                                   const Utils::FilePath &baseDirectory,
                                                                   const QString &localName,
                                                                   const QStringList &extraArgs)
{
    QStringList args;
    args << QLatin1String("clone") << extraArgs << url << localName;
    auto command = new VcsBase::VcsCommand(baseDirectory.toString(),
                                           m_client.processEnvironment());
    command->addJob({m_settings.binaryPath(), args}, -1);
    return command;
}

bool MercurialPluginPrivate::sccManaged(const QString &filename)
{
    const QFileInfo fi(filename);
    QString topLevel;
    const bool managed = managesDirectory(fi.absolutePath(), &topLevel);
    if (!managed || topLevel.isEmpty())
        return false;
    const QDir topLevelDir(topLevel);
    return m_client.manifestSync(topLevel, topLevelDir.relativeFilePath(filename));
}

void MercurialPluginPrivate::changed(const QVariant &v)
{
    switch (v.type()) {
    case QVariant::String:
        emit repositoryChanged(v.toString());
        break;
    case QVariant::StringList:
        emit filesChanged(v.toStringList());
        break;
    default:
        break;
    }
}

    VcsBaseEditorWidget::testDiffFileResolving(dd->diffEditorFactory);
    VcsBaseEditorWidget::testLogResolving(dd->logEditorFactory, data, "18473:692cbda1eb50", "18472:37100f30590f");