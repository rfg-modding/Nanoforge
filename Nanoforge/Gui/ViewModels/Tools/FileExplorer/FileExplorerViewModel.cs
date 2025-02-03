using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Reactive.Linq;
using System.Text.RegularExpressions;
using Avalonia.Controls;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Nanoforge.FileSystem;
using Nanoforge.Gui.Views.Tools.FileExplorer;
using Serilog;

namespace Nanoforge.Gui.ViewModels.Tools.FileExplorer;

public partial class FileExplorerViewModel : Tool
{
    [ObservableProperty]
    private string _statusText = string.Empty;

    [ObservableProperty]
    private bool _statusTextVisible = true;

    [ObservableProperty]
    private string _search = string.Empty;

    [ObservableProperty]
    private ObservableCollection<FileExplorerNodeViewModel> _entries = new();

    private readonly object _entriesLock = new();

    [ObservableProperty]
    private bool _regexSearchMode = false;

    [ObservableProperty]
    private bool _hideUnsupportedFormats = false;

    [ObservableProperty]
    private bool _caseSensitiveSearch = false;

    [ObservableProperty]
    private string _searchError = string.Empty;

    [ObservableProperty]
    private bool _searchInProgress;

    private Regex? _searchRegex = null;

    private IDisposable? _searchObservable;

    private readonly IDisposable _disposeSearchObservable;
    
    private readonly IDisposable _disposeSearchOptionsObservable;

    public FileExplorerViewModel()
    {
        RegexSearchMode = FileExplorerSettings.CVar.Value.RegexSearchMode;
        HideUnsupportedFormats = FileExplorerSettings.CVar.Value.HideUnsupportedFormats;
        CaseSensitiveSearch = FileExplorerSettings.CVar.Value.CaseSensitiveSearch;
        
        if (!Design.IsDesignMode)
        {
            StatusTextVisible = true;
            StatusText = "Waiting for data folder to mount...";
            PackfileVFS.DataFolderChanged += () => { Dispatcher.UIThread.Post(DataFolderChanged); };
        }
        else
        {
            StatusText = string.Empty;
            StatusTextVisible = false;
            SearchError = "Search error test. Long enough to cause TextBlock wrapping.";
            SetDesignerEntries();
        }

        //Uses observable to trigger searches when the search box is changed or any of the search options change
        _disposeSearchObservable = Observable
            .FromEventPattern<PropertyChangedEventHandler, PropertyChangedEventArgs>(handler => PropertyChanged += handler, handler => PropertyChanged -= handler)
            .Where(e => e.EventArgs.PropertyName is nameof(Search))
            .Throttle(TimeSpan.FromMilliseconds(800)) //Wait after key press before beginning a new search. Prevents it from starting a new search on every key press.
            .Subscribe((e) =>
            {
                RegenerateFileTreeAsync();
            });
        
        //Separate observable is used for the search options since we don't need them to be throttled
        _disposeSearchOptionsObservable = Observable
            .FromEventPattern<PropertyChangedEventHandler, PropertyChangedEventArgs>(handler => PropertyChanged += handler, handler => PropertyChanged -= handler)
            .Where(e => e.EventArgs.PropertyName is nameof(RegexSearchMode) or nameof(HideUnsupportedFormats) or nameof(CaseSensitiveSearch))
            .Subscribe((e) =>
            {
                RegenerateFileTreeAsync();
            });
    }

    ~FileExplorerViewModel()
    {
        _disposeSearchObservable.Dispose();
        _disposeSearchOptionsObservable.Dispose();
    }

    private void DataFolderChanged()
    {
        StatusTextVisible = true;
        StatusText = "Loading...";
        RegenerateFileTreeAsync();
    }

    //TODO: Try to do this all through data bindings (including saving CVar changes to disk). Run the tests to make sure it doesn't break NanoDB save/load.
    partial void OnRegexSearchModeChanged(bool value)
    {
        FileExplorerSettings.CVar.Value.RegexSearchMode = value;
        FileExplorerSettings.CVar.Save();
    }

    partial void OnHideUnsupportedFormatsChanged(bool value)
    {
        FileExplorerSettings.CVar.Value.HideUnsupportedFormats = value;
        FileExplorerSettings.CVar.Save();
    }

    partial void OnCaseSensitiveSearchChanged(bool value)
    {
        FileExplorerSettings.CVar.Value.CaseSensitiveSearch = value;
        FileExplorerSettings.CVar.Save();
    }

    private void RegenerateFileTreeAsync()
    {
        //Dispose of any existing search observables so in progress searches results get ignored once a new search is started
        _searchObservable?.Dispose();

        SearchInProgress = true;
        //Note: Observable.Start runs the method passed to it asynchronously
        _searchObservable = Observable.Start<ObservableCollection<FileExplorerNodeViewModel>>(RegenerateFileTree).Subscribe(
            onNext: (entries) =>
            {
                SearchInProgress = false;
                lock (_entriesLock)
                {
                    Entries = entries;
                }
            },

            onError: (ex) =>
            {
                Log.Error(ex, $"File explorer search observable error");
                StatusText = ex.Message;
                StatusTextVisible = true;
            },

            onCompleted: () =>
            {
                StatusText = string.Empty;
                StatusTextVisible = false;
            });
    }

    //Generates file tree from PackfileVFS. Filters by search options.
    private ObservableCollection<FileExplorerNodeViewModel> RegenerateFileTree()
    {
        try
        {
            StatusText = string.Empty;
            StatusTextVisible = false;
            if (PackfileVFS.Root == null)
            {
                StatusText = "Data folder not loaded.";
                StatusTextVisible = true;
                return [];
            }

            SearchError = string.Empty;
            if (RegexSearchMode)
            {
                _searchRegex = null;
                try
                {
                    _searchRegex = new Regex(Search);
                }
                catch (Exception ex)
                {
                    SearchError = ex.Message;
                    return [];
                }
            }

            ObservableCollection<FileExplorerNodeViewModel> entries = new();
            foreach (EntryBase entry in PackfileVFS.Root.Entries)
            {
                FileExplorerNodeViewModel node = BuildNodes(entry, null);
                entries.Add(node);
            }

            return entries;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error generating file explorer tree");
            StatusTextVisible = true;
            StatusText = "Error loading file explorer. Check the log.";
            return [];
        }
    }

    private FileExplorerNodeViewModel BuildNodes(EntryBase vfsEntry, FileExplorerNodeViewModel? parent)
    {
        FileExplorerNodeViewModel node;
        switch (vfsEntry)
        {
            case DirectoryEntry directoryEntry:
            {
                node = new()
                {
                    Type = ExplorerNodeType.Directory,
                    Text = directoryEntry.Name,
                    Parent = parent,
                };

                foreach (EntryBase vfsSubEntry in directoryEntry.Entries)
                {
                    FileExplorerNodeViewModel child = BuildNodes(vfsSubEntry, node);
                    node.Children.Add(child);
                }

                break;
            }

            case FileEntry fileEntry:
            {
                node = new()
                {
                    Type = ExplorerNodeType.File,
                    Text = fileEntry.Name,
                    Parent = parent,
                };

                break;
            }

            default:
                throw new Exception($"Unsupported VFS entry type in file explorer. Entry name: {vfsEntry.Name}");
        }

        //Filter by search term
        if (Search != string.Empty)
        {
            node.AnyChildMatchesSearch = node.Children.Any(child => child.MatchesSearch || child.AnyChildMatchesSearch);
            if (RegexSearchMode)
            {
                node.MatchesSearch = _searchRegex!.IsMatch(node.Text);
            }
            else
            {
                node.MatchesSearch = node.Text.Contains(Search, CaseSensitiveSearch ? StringComparison.CurrentCulture : StringComparison.CurrentCultureIgnoreCase);
            }
        }
        else
        {
            node.AnyChildMatchesSearch = node.Children.Any(child => child.MatchesSearch || child.AnyChildMatchesSearch);
            node.MatchesSearch = true;
        }
        
        if (HideUnsupportedFormats)
        {
            node.MatchesSearch &= FileExplorerView.SupportedFileExtensions.Contains(Path.GetExtension(node.Text));
        }

        return node;
    }

    [RelayCommand]
    private void ResetSearch()
    {
        Search = "";
    }

    private void SetDesignerEntries()
    {
        Entries =
        [
            new FileExplorerNodeViewModel()
            {
                Type = ExplorerNodeType.Directory,
                Text = "test0.vpp_pc",
            },
            new FileExplorerNodeViewModel()
            {
                Type = ExplorerNodeType.Directory,
                Text = "test1.vpp_pc",
                Children =
                [
                    new FileExplorerNodeViewModel()
                    {
                        Type = ExplorerNodeType.File,
                        Text = "File0.xtbl",
                    },
                    new FileExplorerNodeViewModel()
                    {
                        Type = ExplorerNodeType.File,
                        Text = "File1.xtbl",
                    },
                    new FileExplorerNodeViewModel()
                    {
                        Type = ExplorerNodeType.Directory,
                        Text = "container0.str2_pc",
                        Children =
                        [
                            new FileExplorerNodeViewModel()
                            {
                                Type = ExplorerNodeType.File,
                                Text = "file0.cpeg_pc",
                            },
                            new FileExplorerNodeViewModel()
                            {
                                Type = ExplorerNodeType.File,
                                Text = "file0.gpeg_pc",
                            }
                        ],
                    }
                ]
            }
        ];
    }
}