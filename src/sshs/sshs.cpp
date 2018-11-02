#include "sshs_internal.hpp"

#include <boost/tokenizer.hpp>
#include <iostream>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <vector>

class sshs_attribute_updater {
private:
	sshsNode node;
	std::string key;
	enum sshs_node_attr_value_type type;
	sshsAttributeUpdater updater;
	void *userData;

public:
	sshs_attribute_updater(sshsNode _node, const std::string &_key, enum sshs_node_attr_value_type _type,
		sshsAttributeUpdater _updater, void *_userData) :
		node(_node),
		key(_key),
		type(_type),
		updater(_updater),
		userData(_userData) {
	}

	sshsNode getNode() const noexcept {
		return (node);
	}

	const std::string &getKey() const noexcept {
		return (key);
	}

	enum sshs_node_attr_value_type getType() const noexcept {
		return (type);
	}

	sshsAttributeUpdater getUpdater() const noexcept {
		return (updater);
	}

	void *getUserData() const noexcept {
		return (userData);
	}

	// Comparison operators.
	bool operator==(const sshs_attribute_updater &rhs) const noexcept {
		return ((node == rhs.node) && (key == rhs.key) && (type == rhs.type) && (updater == rhs.updater)
				&& (userData == rhs.userData));
	}

	bool operator!=(const sshs_attribute_updater &rhs) const noexcept {
		return (!this->operator==(rhs));
	}
};

// struct for C compatibility
struct sshs_struct {
public:
	sshsNode root;
	std::vector<sshs_attribute_updater> attributeUpdaters;
	std::shared_timed_mutex globalLock;
};

static void sshsGlobalInitialize(void);
static void sshsGlobalErrorLogCallbackInitialize(void);
static void sshsGlobalErrorLogCallbackSetInternal(sshsErrorLogCallback error_log_cb);
static void sshsDefaultErrorLogCallback(const char *msg);
static bool sshsCheckAbsoluteNodePath(const std::string &absolutePath);
static bool sshsCheckRelativeNodePath(const std::string &relativePath);

static sshs sshsGlobal = nullptr;
static std::once_flag sshsGlobalIsInitialized;

static void sshsGlobalInitialize(void) {
	sshsGlobal = sshsNew();
}

sshs sshsGetGlobal(void) {
	std::call_once(sshsGlobalIsInitialized, &sshsGlobalInitialize);

	return (sshsGlobal);
}

static sshsErrorLogCallback sshsGlobalErrorLogCallback = nullptr;
static std::once_flag sshsGlobalErrorLogCallbackIsInitialized;

static void sshsGlobalErrorLogCallbackInitialize(void) {
	sshsGlobalErrorLogCallbackSetInternal(&sshsDefaultErrorLogCallback);
}

static void sshsGlobalErrorLogCallbackSetInternal(sshsErrorLogCallback error_log_cb) {
	sshsGlobalErrorLogCallback = error_log_cb;
}

sshsErrorLogCallback sshsGetGlobalErrorLogCallback(void) {
	std::call_once(sshsGlobalErrorLogCallbackIsInitialized, &sshsGlobalErrorLogCallbackInitialize);

	return (sshsGlobalErrorLogCallback);
}

/**
 * This is not thread-safe, and it's not meant to be.
 * Set the global error callback preferably only once, before using SSHS.
 */
void sshsSetGlobalErrorLogCallback(sshsErrorLogCallback error_log_cb) {
	std::call_once(sshsGlobalErrorLogCallbackIsInitialized, &sshsGlobalErrorLogCallbackInitialize);

	// If nullptr, set to default logging callback.
	if (error_log_cb == nullptr) {
		sshsGlobalErrorLogCallbackSetInternal(&sshsDefaultErrorLogCallback);
	}
	else {
		sshsGlobalErrorLogCallbackSetInternal(error_log_cb);
	}
}

sshs sshsNew(void) {
	sshs newSshs = (sshs) malloc(sizeof(*newSshs));
	sshsMemoryCheck(newSshs, __func__);

	// Create root node.
	newSshs->root = sshsNodeNew("", nullptr, newSshs);

	return (newSshs);
}

bool sshsExistsNode(sshs st, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckAbsoluteNodePath(nodePath)) {
		errno = EINVAL;
		return (false);
	}

	// First node is the root.
	sshsNode curr = st->root;

	// Optimization: the root node always exists.
	if (nodePath == "/") {
		return (true);
	}

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		sshsNode next = sshsNodeGetChild(curr, tok.c_str());

		// If node doesn't exist, return that.
		if (next == nullptr) {
			errno = ENOENT;
			return (false);
		}

		curr = next;
	}

	// We got to the end, so the node exists.
	return (true);
}

sshsNode sshsGetNode(sshs st, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckAbsoluteNodePath(nodePath)) {
		errno = EINVAL;
		return (nullptr);
	}

	// First node is the root.
	sshsNode curr = st->root;

	// Optimization: the root node always exists and is right there.
	if (nodePath == "/") {
		return (curr);
	}

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		sshsNode next = sshsNodeGetChild(curr, tok.c_str());

		// Create next node in path if not existing.
		if (next == nullptr) {
			next = sshsNodeAddChild(curr, tok.c_str());
		}

		curr = next;
	}

	// 'curr' now contains the specified node.
	return (curr);
}

bool sshsExistsRelativeNode(sshsNode node, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckRelativeNodePath(nodePath)) {
		errno = EINVAL;
		return (false);
	}

	// Start with the given node.
	sshsNode curr = node;

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		sshsNode next = sshsNodeGetChild(curr, tok.c_str());

		// If node doesn't exist, return that.
		if (next == nullptr) {
			errno = ENOENT;
			return (false);
		}

		curr = next;
	}

	// We got to the end, so the node exists.
	return (true);
}

sshsNode sshsGetRelativeNode(sshsNode node, const char *nodePathC) {
	const std::string nodePath(nodePathC);

	if (!sshsCheckRelativeNodePath(nodePath)) {
		errno = EINVAL;
		return (nullptr);
	}

	// Start with the given node.
	sshsNode curr = node;

	boost::tokenizer<boost::char_separator<char>> nodePathTokens(nodePath, boost::char_separator<char>("/"));

	// Search (or create) viable node iteratively.
	for (const auto &tok : nodePathTokens) {
		sshsNode next = sshsNodeGetChild(curr, tok.c_str());

		// Create next node in path if not existing.
		if (next == nullptr) {
			next = sshsNodeAddChild(curr, tok.c_str());
		}

		curr = next;
	}

	// 'curr' now contains the specified node.
	return (curr);
}

void sshsAttributeUpdaterAdd(sshsNode node, const char *key, enum sshs_node_attr_value_type type,
	sshsAttributeUpdater updater, void *updaterUserData) {
	sshs_attribute_updater attrUpdater(node, key, type, updater, updaterUserData);

	sshs tree = sshsNodeGetGlobal(node);
	std::unique_lock<std::shared_timed_mutex> lock(tree->globalLock);

	// Check no other updater already exists that matches this one.
	if (!findBool(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(), attrUpdater)) {
		tree->attributeUpdaters.push_back(attrUpdater);
	}
}

void sshsAttributeUpdaterRemove(sshsNode node, const char *key, enum sshs_node_attr_value_type type,
	sshsAttributeUpdater updater, void *updaterUserData) {
	sshs_attribute_updater attrUpdater(node, key, type, updater, updaterUserData);

	sshs tree = sshsNodeGetGlobal(node);
	std::unique_lock<std::shared_timed_mutex> lock(tree->globalLock);

	tree->attributeUpdaters.erase(
		std::remove(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(), attrUpdater),
		tree->attributeUpdaters.end());
}

void sshsAttributeUpdaterRemoveAllForNode(sshsNode node) {
	sshs tree = sshsNodeGetGlobal(node);
	std::unique_lock<std::shared_timed_mutex> lock(tree->globalLock);

	tree->attributeUpdaters.erase(std::remove_if(tree->attributeUpdaters.begin(), tree->attributeUpdaters.end(),
									  [&node](const sshs_attribute_updater &up) { return (up.getNode() == node); }),
		tree->attributeUpdaters.end());
}

void sshsAttributeUpdaterRemoveAll(sshs tree) {
	std::unique_lock<std::shared_timed_mutex> lock(tree->globalLock);

	tree->attributeUpdaters.clear();
}

bool sshsAttributeUpdaterRun(sshs tree) {
	std::shared_lock<std::shared_timed_mutex> lock(tree->globalLock);

	bool allSuccess = true;

	for (const auto &up : tree->attributeUpdaters) {
		union sshs_node_attr_value newValue = (*up.getUpdater())(up.getUserData(), up.getKey().c_str(), up.getType());

		if (!sshsNodePutAttribute(up.getNode(), up.getKey().c_str(), up.getType(), newValue)) {
			allSuccess = false;
		}
	}

	return (allSuccess);
}

#define ALLOWED_CHARS_REGEXP "([a-zA-Z-_\\d\\.]+/)"
static const std::regex sshsAbsoluteNodePathRegexp("^/" ALLOWED_CHARS_REGEXP "*$");
static const std::regex sshsRelativeNodePathRegexp("^" ALLOWED_CHARS_REGEXP "+$");

static bool sshsCheckAbsoluteNodePath(const std::string &absolutePath) {
	if (absolutePath.empty()) {
		(*sshsGetGlobalErrorLogCallback())("Absolute node path cannot be empty.");
		return (false);
	}

	if (!std::regex_match(absolutePath, sshsAbsoluteNodePathRegexp)) {
		boost::format errorMsg = boost::format("Invalid absolute node path format: '%s'.") % absolutePath;

		(*sshsGetGlobalErrorLogCallback())(errorMsg.str().c_str());

		return (false);
	}

	return (true);
}

static bool sshsCheckRelativeNodePath(const std::string &relativePath) {
	if (relativePath.empty()) {
		(*sshsGetGlobalErrorLogCallback())("Relative node path cannot be empty.");
		return (false);
	}

	if (!std::regex_match(relativePath, sshsRelativeNodePathRegexp)) {
		boost::format errorMsg = boost::format("Invalid relative node path format: '%s'.") % relativePath;

		(*sshsGetGlobalErrorLogCallback())(errorMsg.str().c_str());

		return (false);
	}

	return (true);
}

static void sshsDefaultErrorLogCallback(const char *msg) {
	std::cerr << msg << std::endl;
}
