const { Session } = require('deplug')
const m = require('mithril')
class Enum {
  view (vnode) {
    const { value } = vnode.attrs.prop
    const flags = vnode.attrs.layer.attrs
      .filter((prop) => prop.id.startsWith(`${vnode.attrs.prop.id}.`))
      .map((prop) => {
        const { id } = prop
        return (id in Session.attributes)
          ? Session.attributes[id].name
          : id
    })
    .join(', ')
    return m('span', [flags, ' (', value, ')'])
  }
}
module.exports = Enum