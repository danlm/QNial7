# Contributing to Q'Nial

**Contents**

* [Issue Contributions](#issue-contributions)
* [Code Contributions](#code-contributions)
 - [Code Style Guidelines](#code-style-guidelines)
 - [Contribution Submission](#contribution-submission)
 - [Contribution Approval](#contribution-approval)

## Issue Contributions

When opening new issues or commenting on existing issues on this repository
please make sure discussions are related to concrete technical issues with the
Q'Nial software.  For other topics, please post to the project forums.

## Code Contributions

The Q'Nial project has an open governance model and welcomes new contributors.
Individuals making significant and valuable contributions are made
_Collaborators_ and given commit-access to the project.

### Code Style

[TBA]

### Contribution Submission

This document will guide you through the contribution process.

### Step 1: Fork

Fork the project [on GitHub] and check out your
copy locally.

```text
$ git clone git@github.com:[TBD]
$ cd node
$ git remote add upstream [TBD]
```

#### Which branch?

For developing new features and bug fixes, the `master` branch should be pulled
and built upon.

#### Dependencies

Nial has several bundled dependencies
that are not part of the project proper. Any changes to files
in those directories or its subdirectories should be sent to their respective
projects. Do not send your patch to us, we cannot accept it.

In case of doubt, open an issue or contact one of the project collaborators, especially if you plan to work on something big. Nothing is more
frustrating than seeing your hard work go to waste because your vision
does not align with the project team.


### Step 2: Branch

Create a branch and start hacking:

```text
$ git checkout -b my-branch -t origin/master
```

### Step 3: Commit

Make sure git knows your name and email address:

```text
$ git config --global user.name "J. Random User"
$ git config --global user.email "j.random.user@example.com"
```

Writing good commit logs is important. A commit log should describe what
changed and why. Follow these guidelines when writing one:

1. The first line should be 50 characters or less and contain a short
   description of the change. All words in the description should be in
   lowercase with the exception of proper nouns, acronyms, and the ones that
   refer to code, like function/variable names. The description should
   be prefixed with the name of the changed subsystem and start with an
   imperative verb, for example, "net: add localAddress and localPort
   to Socket".
2. Keep the second line blank.
3. Wrap all other lines at 72 columns.

A good commit log can look something like this:

```txt
subsystem: explain the commit in one line

Body of commit message is a few lines of text, explaining things
in more detail, possibly giving some background about the issue
being fixed, etc. etc.

The body of the commit message can be several paragraphs, and
please do proper word-wrap and keep columns shorter than about
72 characters or so. That way `git log` will show things
nicely even when it is indented.
```

The header line should be meaningful; it is what other people see when they
run `git shortlog` or `git log --oneline`.

Check the output of `git log --oneline files_that_you_changed` to find out
what subsystem (or subsystems) your changes touch.

If your patch fixes an open issue, you can add a reference to it at the end
of the log. Use the `Fixes:` prefix and the full issue URL. For example:

```txt
Fixes: https://github.com/nodejs/node/issues/1337
```

### Step 4: Rebase

Use `git rebase` (not `git merge`) to sync your work from time to time.

```text
$ git fetch upstream
$ git rebase upstream/master
```

### Step 5: Test

[TBA]

### Step 6: Push

```text
$ git push origin my-branch
```

Go to https://github.com/yourusername/nial-dev and select your branch.
Click the 'Pull Request' button and fill out the form.

### Contribution Approval Policy

Pull requests are usually reviewed within a few days. If there are comments
to address, apply your changes in a separate commit and push that to your
branch. Post a comment in the pull request afterwards; GitHub does
not send out notifications when you add commits.

For changes to the core of the system, the approval will first be vetted
for appropriateness as well as value to the system.




