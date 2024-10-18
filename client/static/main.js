function sortByKey(array, key) {
    return array.sort(function (a, b) {
        let x = a[key];
        let y = b[key];
        return ((x > y) ? -1 : ((x < y) ? 1 : 0));
    });
}

function sortTree(ul, key, cb) {
    if (cb.checked) {
        let uls = document.getElementsByClassName(ul);
        for (let i = 0; i < uls.length; i++) {
            let children = uls[i].children;
            let liChildren = [];
            for (let j = 0; j < children.length; j++) {
                if (children[j].tagName === "LI") {
                    liChildren.push(children[j]);
                }
            }
            Array.from(liChildren)
                .sort(function (a, b) {
                    let x = parseInt(a.getElementsByClassName(key)[0].getAttribute('data-value'));
                    let y = parseInt(b.getElementsByClassName(key)[0].getAttribute('data-value'));
                    return y - x;
                })
                .forEach(li => uls[i].appendChild(li));
        }
    }
}

function deleteElements(ul, key, cb) {
    let uls = document.getElementsByClassName(ul);
    for (let i = 0; i < uls.length; i++) {
        Array.from(uls[i].children).forEach(li => {
            if (li.getElementsByClassName(key).length === 0) {
                if (cb.checked
                    && li.getElementsByClassName('bin command').length === 0
                    && li.getElementsByClassName('open_len').length === 0) {
                    li.style.display = 'none';
                } else {
                    li.style.display = 'block';
                }
            }
        });
    }
}

function nanoToStr(ns_time) {
    if (ns_time === 0) {
        return "-"
    }
    let secs = Math.floor(ns_time / 1000000000);
    let ret = ""
    let h = Math.floor((secs %= 86400) / 3600);
    if (h) {
        ret += h + 'h ';
    }
    let m = Math.floor((secs %= 3600) / 60);
    if (m) {
        ret += m + 'm ';
    }
    let s = secs % 60;
    if (s) {
        ret += s + 's ';
    }
    if (ret === "") {
        let ms = Math.floor(ns_time / 1000000);
        let mseconds = ms % 1000;
        if (mseconds) {
            ret += mseconds + 'ms ';
        } else {
            ret += "0." + Math.floor(ns_time / 1000) + 'ms ';
        }
    }
    return ret;
}

async function fillChild() {
    let pidIdx = $(this)[0].id.split(":");
    let pid = pidIdx[0];
    let idx = pidIdx[1];
    get_childs(pid, idx, this);
}

function search_renderer(proc, node, highestParent = false) {
    let li = ($('<li/>'));
    li.data("parent", proc['ppid'] + ":" + proc['pidx']);
    let div = $('<div/>');
    let caret = ($('<button />')).text(proc['pid'] + ":" + proc['idx']);
    caret.attr('id', proc['pid'] + ":" + proc['idx']);
    caret.addClass('pid col-1 btn btn-sm me-1');
    if (node.getElementsByTagName("ul").length <= 1 || highestParent) {
        div.addClass('no-before');
    }
    if (proc['children'] > 0) {
        caret.addClass('caret');
        caret.addClass('btn-primary');
        let child_len = $('<span class="badge rounded-pill bg-danger child_len"/>').text(proc['children']);
        caret.append(child_len);
    } else {
        caret.addClass('btn-outline-primary');
        if ($("#nonChildFilter")[0].checked && proc['bin'] === "" && proc['open_len'] === 0) {
            li[0].style.display = 'none';
        }
        caret.addClass('nonCaret');
        caret.attr('disabled', 'disabled');
    }
    if (proc['open_len']) {
        let open_len = $('<span class="badge rounded-pill bg-success open_len"/>').text(proc['open_len']);
        caret.append(open_len);
    }
    div.append(caret);
    div.append($(`<button type="button" class="btn btn-outline-primary btn-sm" data-bs-toggle="modal" data-bs-target="#proc_info_modal" data-len=${proc['open_len']} data-bs-pid="${proc["pid"]}:${proc["idx"]}">i</button>`));

    div.append(($(`<span class="etime" data-value=${proc['etime']} />`)).text(nanoToStr(proc['etime'])));
    if (proc['bin']) {
        div.append(($('<span class="me-1"/>')).text(" bin:"));
        div.append(($(`<span class="bin ${proc['class']}" />`)).text(proc['bin']));
    }
    if (proc["children"] > 0) {
        div.append($('<span />').append($(`<a href="${ctx}/proc_tree?pid=${proc["pid"]}&idx=${proc["idx"]}" target="_blank" title="Start from this process" style="text-decoration:none" />`).text(" ↸ ")));
    }
    li.append(div);
    return li;
}

function get_childs(pid, idx, node, page = 0) {
    if(document.getElementById('nav'+pid+':'+idx)){
        document.getElementById('nav'+pid+':'+idx).remove();
    }
    let children_url = `${ctx}/children?pid=${pid}&idx=${idx}&page=${page}`;
    let etime_sorting_selected = document.getElementById("etimeSorting").checked;
    if (etime_sorting_selected) {
        children_url += "&etime_sort=true";
    }
    let hide_empty_children = document.getElementById("nonChildFilter").checked;
    if (hide_empty_children) {
        children_url += "&hide_empty=true";
    }

    const request = new XMLHttpRequest();
    request.open("GET", children_url, false);
    request.send(null);

    if (request.status === 200) {
    }

    let data = JSON.parse(request.responseText);
    let process = $('<ul class="nested" />');
    if (parseInt(data["pages"]) > 1) {
        let divPag = $('<div class="row  align-middle ml-10"/>');
        divPag.attr('id', 'nav'+pid+':'+idx);
        let navPag = $('<nav class="ml-20 col-md-auto"/>');
        let ulPag = $('<ul class="pagination pagination-sm"/>');
        let liPag = $('<li class="page-item "/>');
        liPag.addClass(page > 0 ? "" : "disabled");
        let buttonPag = $('<button class="page-link"/>');
        buttonPag.text("« First");
        buttonPag.click(function () {
            let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
            let lis = ul.children;
            let toSave = [];
            for (let linum = 0; linum < lis.length; linum++) {
                if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                    toSave.push(lis[linum])
                }
            }
            ul.innerHTML = '';
            toSave.forEach(element => ul.append(element));
            get_childs(pid, idx, node, 0);
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);
        liPag = $('<li class="page-item "/>');
        liPag.addClass(page > 0 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("‹ Previous");
        buttonPag.click(function () {
            if (page > 0) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                
                get_childs(pid, idx, node, page-1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        if(page - 1 > 0){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page-2);
            buttonPag.click(function () {
                if (page > 1) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(pid, idx, node, page-2);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        if(page  > 0){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page-1);
            buttonPag.click(function () {
                if (page > 0) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(pid, idx, node, page-1);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        liPag = $('<li class="page-item "/>');
        liPag.addClass("active");
        liPag.attr("aria-current", "page");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text(page);
        buttonPag.click(function () {
            let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
            let lis = ul.children;
            let toSave = [];
            for (let linum = 0; linum < lis.length; linum++) {
                if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                    toSave.push(lis[linum])
                }
            }
            ul.innerHTML = '';
            toSave.forEach(element => ul.append(element));
            get_childs(pid, idx, node, page);
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        if(page < data.pages-1){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page+1);
            buttonPag.click(function () {
                if (page < (parseInt(data["pages"]) - 1)) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(pid, idx, node, page+1);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }

        if(page+1 < data.pages-1){
            liPag = $('<li class="page-item "/>');
            buttonPag = $('<button class="page-link"/>');
            buttonPag.text(page+2);
            buttonPag.click(function () {
                if (page < (parseInt(data["pages"]) - 2)) {
                    let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                    let lis = ul.children;
                    let toSave = [];
                    for (let linum = 0; linum < lis.length; linum++) {
                        if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                            toSave.push(lis[linum])
                        }
                    }
                    ul.innerHTML = '';
                    toSave.forEach(element => ul.append(element));
                    get_childs(pid, idx, node, page+2);
                }
            });
            liPag.append(buttonPag);
            ulPag.append(liPag);
        }
        liPag = $('<li class="page-item "/>');
        liPag.addClass(page < data.pages-1 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("Next ›");
        buttonPag.click(function () {
            if (page < (parseInt(data["pages"]) - 1)) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                get_childs(pid, idx, node, page+1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        liPag = $('<li class="page-item "/>');
        liPag.addClass(page < data.pages-1 ? "" : "disabled");
        buttonPag = $('<button class="page-link"/>');
        buttonPag.text("Last »");
        buttonPag.click(function () {
            if (page < (parseInt(data["pages"]) - 1)) {
                let ul = node.parentNode.parentNode.getElementsByClassName("nested")[0];
                let lis = ul.children;
                let toSave = [];
                for (let linum = 0; linum < lis.length; linum++) {
                    if (lis[linum].getElementsByClassName("nested").length > 0 && lis[linum].getElementsByClassName("nested")[0].style.display !== "none") {
                        toSave.push(lis[linum])
                    }
                }
                ul.innerHTML = '';
                toSave.forEach(element => ul.append(element));
                get_childs(pid, idx, node, data.pages-1);
            }
        });
        liPag.append(buttonPag);
        ulPag.append(liPag);

        navPag.append(ulPag);
        divPag.append(navPag);
        let spanPag = $('<span class = "col-md-auto" style="margin-top:10px"/>');
        spanPag.append($('<span style="font-weight:100"/>').text("Showing "));
        spanPag.append(page * 20 + 1+'-'+Math.min((page+1) * 20, data.count));
        spanPag.append($('<span style="font-weight:100"/>').text(" of "));
        spanPag.append(data.count);
        divPag.append(spanPag);
        node.parentNode.parentNode.firstChild.after(divPag[0]);
    }
    if (node.parentNode.parentNode.getElementsByClassName("nested").length) {
        process = $(node.parentNode.parentNode.getElementsByClassName("nested")[0]);
    } else {
        process.appendTo(node.parentNode.parentNode);
    }
    let childrenArray;
    if (etime_sorting_selected) {
        childrenArray = sortByKey(data["children"], 'etime');
    } else {
        childrenArray = data["children"];
    }
    for (let i = 0; i < childrenArray.length; i++) {
        if (document.getElementById(childrenArray[i]['pid'] + ":" + childrenArray[i]['idx'])) {
            continue;
        }
        let li = $('<li/>');
        let div = $('<div/>');
        let caret = ($('<button />')).text(childrenArray[i]['pid'] + ":" + childrenArray[i]['idx']);
        caret.attr('id', childrenArray[i]['pid'] + ":" + childrenArray[i]['idx']);
        caret.addClass('pid col-1 btn btn-sm me-1');

        if (childrenArray[i]['children'] > 0) {
            caret.addClass('caret');
            caret.addClass('btn-primary');
            let child_len = $('<span class="badge rounded-pill bg-danger child_len"/>').text(childrenArray[i]['children']);
            caret.append(child_len);
        } else {
            caret.addClass('btn-outline-primary');
            if ($("#nonChildFilter")[0].checked && childrenArray[i]['bin'] === "" && childrenArray[i]['open_len'] === 0) {
                li[0].style.display = 'none';
            }
            caret.addClass('nonCaret');
            caret.attr('disabled', 'disabled');
        }
        if (childrenArray[i]['open_len']) {
            let open_len = $('<span class="badge rounded-pill bg-success open_len"/>').text(childrenArray[i]['open_len']);
            caret.append(open_len);
        }
        div.append(caret);
        div.append($('<button type="button" class="btn btn-outline-primary btn-sm" data-bs-toggle="modal" data-bs-target="#proc_info_modal" data-len=' + childrenArray[i]['open_len'] + ' data-bs-pid="' + childrenArray[i]["pid"] + ":" + childrenArray[i]["idx"] + '">i</button>'));

        div.append(($('<span class="etime" data-value=' + childrenArray[i]['etime'] + ' />')).text(nanoToStr(childrenArray[i]['etime'])));
        if (childrenArray[i]['bin']) {
            div.append(($('<span class="me-1" />')).text("bin:"));
            div.append(($('<span class="bin ' + childrenArray[i]['class'] + '" />')).text(childrenArray[i]['bin']));
        }
        if (childrenArray[i]["children"] > 0) {
            div.append($('<span />').append($(`<a href="${ctx}/proc_tree?pid=${childrenArray[i]["pid"]}&idx=${childrenArray[i]["idx"]}" target="_blank" title="Start from this process" style="text-decoration:none" />`).text(" ↸ ")));
        }
        li.append(div);
        process.append(li);
    }

    $(node).removeClass('caret');
    $(node).addClass('caretDone');
}

$(document).ready(function () {
    let time_labels = document.getElementsByClassName("etime");
    for (let i = 0; i < time_labels.length; i++) {
        time_labels.item(i).style = null;
        time_labels.item(i).innerText = nanoToStr(time_labels.item(i).innerText);
    }
    let b = $("body");

    b.unbind().on("click", ".caret", fillChild);

    b.on("click", ".caretDone", async function () {
        if(document.getElementById('nav'+this.id)){
            document.getElementById('nav'+this.id).style.display = "none";
        }
        let parent = this.parentNode.parentNode;
        let childrenList = $(parent).children('.nested');
        $(childrenList[0]).css("display", "none");
        $(this).addClass('caretReady');
        $(this).removeClass('caretDone');
    });

    b.on("click", ".caretReady", async function () {
        if(document.getElementById('nav'+this.id)){
            document.getElementById('nav'+this.id).style.display = "";
        }
        let parent = this.parentNode.parentNode;
        let childrenList = $(parent).children('.nested');
        $(childrenList[0]).css("display", "block");
        $(this).removeClass('caretReady');
        $(this).addClass('caretDone');
    });

});

function upperParent(elem) {
    let child = elem.parentNode.parentNode;
    if (!document.getElementById($(elem).data('value'))) {
        let process_url = `${ctx}/proc_lookup?pid=${$(elem).data('value').split(":")[0]}&idx=${$(elem).data('value').split(":")[1]}`;
        const request = new XMLHttpRequest();
        request.open("GET", process_url, false);
        request.send(null);

        if (request.status === 200) {
        }

        let proc = JSON.parse(request.responseText);
        let newNode = search_renderer(proc, elem.parentNode.parentNode.parentNode, true);
        let parentValue = newNode.data("parent")
        if (parentValue.split(":")[0] !== '0') {
            let a = $('<a onclick="upperParent(this)" class="upper_parent" data-value=' + parentValue + ' />').text("⇖");
            $(newNode)[0].firstChild.prepend(a[0]);
        }
        let process = $('<ul class="nested" />');
        elem.remove();
        child.replaceWith(newNode[0]);
        $(child.getElementsByClassName("no-before")[0]).removeClass("no-before");
        process.appendTo(newNode);
        $(child).appendTo(process);
    } else {
        let newNode = document.getElementById($(elem).data('value'));
        $(child.firstChild).removeClass("no-before");
        elem.remove();
        child.remove();
        if (newNode.parentNode.parentNode.getElementsByClassName('nested').length) {
            $(child).appendTo(newNode.parentNode.parentNode.getElementsByClassName('nested')[0]);
        } else {
            let process = $('<ul class="nested" />');
            process.appendTo(newNode.parentNode.parentNode);
            $(child).appendTo(process);
        }
    }
}

function search(page = 0) {
    let bin_cwd_cmd, wc_re_m, class_name;
    let maxResults = 20
    let search_text = document.getElementById("search_text").value;
    let ele = document.getElementsByName("filter_subject");
    for (let i = 0; i < ele.length; i++) {
        if (ele[i].checked)
            bin_cwd_cmd = ele[i].value;
    }
    ele = document.getElementsByName("filter_type");
    for (let i = 0; i < ele.length; i++) {
        if (ele[i].checked)
            wc_re_m = ele[i].value;
    }
    let opened_files = document.getElementById("path_text").value;
    let src_root = document.getElementById("filter_src_root").checked;
    ele = document.getElementsByName("filter_class");
    for (let i = 0; i < ele.length; i++) {
        if (ele[i].checked)
            class_name = ele[i].value;
    }
    let negate = document.getElementById("filter_negate").checked;
    let search_url = `${ctx}/search?`;

    let filterString = ""
    if (bin_cwd_cmd && search_text && wc_re_m){
        filterString += bin_cwd_cmd + '=' + search_text + ',type=' + wc_re_m;
    }
    if (src_root) {
        if (filterString.length>0) filterString += ",";
        filterString += 'bin_source_root=' + src_root;
    }
    if (class_name !== '*') {
        if (filterString.length>0) filterString += ",";
        filterString += 'class=' + class_name;
    }
    if (negate) {
        if (filterString.length>0) filterString += ",";
        filterString += 'negate=' + negate;
    }
    if (filterString.length > 0){
        filterString = 'filter=[' + filterString +  ']'
    }
    
    if (opened_files) {
        search_url += 'filename=' + opened_files + (filterString.length > 0 ? ('&' + filterString) : "" );
    } else {
        search_url += filterString.length > 0 ? ('&' + filterString) : "";
    }
    search_url += '&page=' + page + '&entries=' + maxResults;
    if($("#etimeSorting")[0].checked){
        search_url += "&etime_sort=true"
    }

    const request = new XMLHttpRequest();
    request.open("GET", search_url, false);
    request.send(null);

    if (request.status === 200) {
    }
    let trees = document.getElementsByClassName('treeview-animated-list mb-3');
    for (let i = 0; i < trees.length; i++) {
        trees[i].innerHTML = '';
    }
    let node = trees[0];
    let data = JSON.parse(request.responseText);
    document.getElementById("data-len").innerText = data.count;
    if (data.count > maxResults) {
        render_pagination_nohref("tree_pagination", page, Math.floor(data.count/maxResults), data.count, maxResults);
    }

    for (let i = 0; i < data["execs"].length; i++) {
        let newElem = search_renderer(data["execs"][i], node);
        newElem.appendTo(node);
        if (data["execs"][i]['ppid'] > 0) {
            let a = $(`<a onclick="upperParent(this)" class="upper_parent" data-value="${data["execs"][i]['ppid']}:${data["execs"][i]['pidx']}" />`).text("⇖");
            newElem[0].firstChild.prepend(a[0]);
        }
    }
}

function reset_e() {
    document.getElementById("data-len").innerText = "0";
    document.getElementById("data-idx").innerText = "0";
}


function render_pagination(container_id, cur_page, max_page, count, entries_per_page, origin_url){
    cur_page = parseInt(cur_page, 10);
    max_page = parseInt(max_page, 10);
    count = parseInt(count, 10);
    entries_per_page = parseInt(entries_per_page, 10);
    let pagination_nav = document.getElementById(container_id);

    pagination_nav.innerHTML = `<nav class="ml-20 col-md-auto"><ul class="pagination pagination-sm">

    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page=0')}">« First</a>
    </li>
    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page - 1))}">‹ Previous</a>
    </li>` +
    ((cur_page - 1) > 0 ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page - 2))}">${cur_page - 2}</a>
    </li>
    `: ``) +
    (cur_page > 0 ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page - 1))}">${cur_page - 1}</a>
    </li>
    `: ``) +
    `<li class="page-item active" aria-current="page">
        <a class="page-link" href="#">${cur_page}</a>
    </li>
    `+
    (cur_page < max_page ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page + 1))}">${cur_page + 1}</a>
    </li>
    `: ``) +
    ((cur_page + 1) < max_page ? `
    <li class="page-item">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page + 2))}">${cur_page + 2}</a>
    </li>
    `: ``) +
    `<li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (cur_page + 1))}">Next ›</a>
    </li>
    <li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <a class="page-link" href="${ origin_url.replace('&amp;page='+ cur_page ,'&amp;page='+ (max_page))}">Last »</a>
    </li>
</ul></nav>
 <span class="col-md-auto"><span style="font-weight:100">Showing</span> ${cur_page * entries_per_page + 1}-${ Math.min((cur_page+1) * entries_per_page, count) } <span style="font-weight:100">of</span> ${count}</span>
`;
    pagination_nav.style.display = "";
}
function render_pagination_nohref(container_id, cur_page, max_page, count, entries_per_page){
    cur_page = parseInt(cur_page, 10);
    max_page = parseInt(max_page, 10);
    count = parseInt(count, 10);
    entries_per_page = parseInt(entries_per_page, 10);
    let pagination_nav = document.getElementById(container_id);

    pagination_nav.innerHTML = `<nav class="ml-20 col-md-auto"><ul class="pagination pagination-sm">

    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <button class="page-link" onclick="search(${page=0})">« First</button>
    </li>
    <li class="page-item ${cur_page > 0 ? '' :' disabled' }">
        <button class="page-link" onclick="search(${cur_page - 1})">‹ Previous</button>
    </li>` +
    ((cur_page - 1) > 0 ? `
    <li class="page-item">
        <button class="page-link" onclick="search(${cur_page - 2})">${cur_page - 2}</button>
    </li>
    `: ``) +
    (cur_page > 0 ? `
    <li class="page-item">
        <button class="page-link" onclick="search(${cur_page - 1})">${cur_page - 1}</button>
    </li>
    `: ``) +
    `<li class="page-item active" aria-current="page">
        <button class="page-link" onclick="#">${cur_page}</button>
    </li>
    `+
    (cur_page < max_page ? `
    <li class="page-item">
        <button class="page-link" onclick="search(${cur_page + 1})">${cur_page + 1}</button>
    </li>
    `: ``) +
    ((cur_page + 1) < max_page ? `
    <li class="page-item">
        <button class="page-link" onclick="search(${cur_page + 2})">${cur_page + 2}</button>
    </li>
    `: ``) +
    `<li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <button class="page-link" onclick="search(${cur_page + 1})">Next ›</button>
    </li>
    <li class="page-item` + (cur_page < max_page ? '' :' disabled') +`">
        <button class="page-link" onclick="search(${max_page})">Last »</button>
    </li>
</ul></nav>
 <span class="col-md-auto"><span style="font-weight:100">Showing</span> ${cur_page * entries_per_page + 1}-${ Math.min((cur_page+1) * entries_per_page, count) } <span style="font-weight:100">of</span> ${count}</span>
`;
    pagination_nav.style.display = "";
}
